// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSFixedCameraVolume.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ChronoSpace.h"

ACSFixedCameraVolume::ACSFixedCameraVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSFixedCameraVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSFixedCameraVolume::OnTriggerEndOverlap);

	// 고정 카메라 컴포넌트 — 에디터에서 위치/회전 조정 가능
	FixedCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FixedCamera"));
	FixedCamera->SetupAttachment(RootComponent);
	FixedCamera->SetRelativeLocation(FVector(0.f, -800.f, 200.f));
	FixedCamera->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
}

void ACSFixedCameraVolume::BeginPlay()
{
	Super::BeginPlay();
	LocalTriggerCharacters.Empty();
	LockedControllerByCharacter.Empty();
}

void ACSFixedCameraVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (auto It = ControlRotationLerpStates.CreateIterator(); It; ++It)
	{
		APlayerController* PC = It->Key;
		FControlRotationLerpState& State = It->Value;

		if (!IsValid(PC) || !State.bIsLerping)
		{
			It.RemoveCurrent();
			continue;
		}

		State.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(State.Elapsed / State.Duration, 0.f, 1.f);
		const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

		const FRotator NewRotation = FMath::Lerp(State.StartRotation, State.TargetRotation, SmoothedAlpha);
		PC->SetControlRotation(NewRotation);

		if (Alpha >= 1.f)
		{
			State.bIsLerping = false;
			It.RemoveCurrent();
		}
	}
}

void ACSFixedCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	// ── 카메라 전환 (로컬 플레이어만) ──
	if (Character->IsLocallyControlled())
	{
		// 어떤 PC 를 잠갔는지 캐릭터별로 기억한다.
		// 사망/언포제스로 EndOverlap 시점에 Character->GetController() 가 null 이면
		// SetIgnoreLookInput(true) 가 풀리지 않아 시점 입력이 영구히 막혔다.
		LockedControllerByCharacter.Add(Character, PC);

		PC->SetViewTargetWithBlend(this, BlendTime);

		// 컨트롤러 회전 저장
		SavedControlRotations.Add(PC, PC->GetControlRotation());

		// ControlRotation은 Lerp로 부드럽게 전환 (이동 방향이 즉시 바뀌지 않음)
		FControlRotationLerpState LerpState;
		LerpState.StartRotation = PC->GetControlRotation();
		LerpState.TargetRotation = FixedControlRotation;
		LerpState.Elapsed = 0.f;
		LerpState.Duration = ControlRotationBlendTime;
		LerpState.bIsLerping = true;
		ControlRotationLerpStates.Add(PC, LerpState);

		PC->SetIgnoreLookInput(true);
	}

	// ── 스플릿 스크린 전환 (옵션) ── 이 머신의 화면만 바뀌므로 로컬 판정이 필요하다
	if (bUseSplitScreenTransition
		&& UCSSplitScreenSubsystem::ShouldLocalViewRespondTo(Character, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		LocalTriggerCharacters.Add(Character);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->RequestFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: local player entered -> Full Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: %s entered → Fixed camera, ControlRotation locked"), *Character->GetName());
}

void ACSFixedCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());

	// 진입 때 잠갔던 PC 를 되찾는다 (컨트롤러가 이미 떨어졌어도 입력 잠금은 풀어야 한다)
	bool bWasLockedByUs = false;
	if (TWeakObjectPtr<APlayerController>* Found = LockedControllerByCharacter.Find(Character))
	{
		if (!PC) PC = Found->Get();
		bWasLockedByUs = true;
		LockedControllerByCharacter.Remove(Character);
	}
	for (auto PurgeIt = LockedControllerByCharacter.CreateIterator(); PurgeIt; ++PurgeIt)
	{
		if (!PurgeIt->Key.IsValid())
		{
			PurgeIt.RemoveCurrent();
		}
	}

	// ── 카메라 복원 (진입 때 우리가 잠근 경우) ──
	if (PC && bWasLockedByUs)
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		// ── 나갈 때의 ControlRotation ──
		// 이동 방향은 ControlRotation 의 Yaw 기준이다. 예전엔 "진입 전 회전"으로 되돌려서,
		// 볼륨 안에서 가던 방향으로 계속 가려 해도 나가는 순간 이동 방향이 튀었다.
		// 기본 동작: 보고 있던 Yaw 를 그대로 유지하고 Pitch/Roll 만 진입 전 값으로 되돌린다
		// (고정 카메라가 아래를 내려다보는 각이어도 3인칭 카메라가 기울어진 채로 남지 않게).
		const FRotator* SavedRot = SavedControlRotations.Find(PC);
		const FRotator CurrentRot = PC->GetControlRotation();

		FRotator TargetRot = CurrentRot;
		if (bRestoreControlRotationOnExit)
		{
			if (SavedRot) TargetRot = *SavedRot;
		}
		else
		{
			TargetRot.Pitch = SavedRot ? SavedRot->Pitch : 0.f;
			TargetRot.Roll = SavedRot ? SavedRot->Roll : 0.f;
		}

		if (!TargetRot.Equals(CurrentRot, 0.01f))
		{
			FControlRotationLerpState LerpState;
			LerpState.StartRotation = CurrentRot;
			LerpState.TargetRotation = TargetRot;
			LerpState.Elapsed = 0.f;
			LerpState.Duration = ControlRotationBlendTime;
			LerpState.bIsLerping = true;
			ControlRotationLerpStates.Add(PC, LerpState);
		}
		else
		{
			// 목표가 현재와 같으면 진행 중이던 Lerp 만 멈춘다 (진입 Lerp 가 남아 되돌아가지 않게)
			ControlRotationLerpStates.Remove(PC);
		}

		SavedControlRotations.Remove(PC);

		PC->SetIgnoreLookInput(false);
	}

	const bool bWasLocalTrigger = (LocalTriggerCharacters.Remove(Character) > 0);
	// 파괴된 캐릭터(사망 등)의 약참조는 Remove(nullptr) 로 지워지지 않는다.
	// 인덱스/시리얼이 남아 있어 null 약참조와 같지 않기 때문 - 그대로 두면 Num() 이 0 이 되지 않아
	// 볼륨을 나가도 스플릿으로 영영 복귀하지 못한다.
	for (auto PurgeIt = LocalTriggerCharacters.CreateIterator(); PurgeIt; ++PurgeIt)
	{
		if (!PurgeIt->IsValid())
		{
			PurgeIt.RemoveCurrent();
		}
	}

	// ── 스플릿 스크린 복원 (우리 화면을 바꾼 플레이어가 모두 나갔을 때) ──
	if (bUseSplitScreenTransition && bWasLocalTrigger && LocalTriggerCharacters.Num() == 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->ReleaseFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: All players left → Split Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: %s exited → Restore 3rd-person camera"), *Character->GetName());
}
