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
				Subsystem->TransitionToFullScreen(0);
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
	LockedControllerByCharacter.Remove(nullptr);

	// ── 카메라 복원 (진입 때 우리가 잠근 경우) ──
	if (PC && bWasLockedByUs)
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		// ControlRotation도 Lerp로 부드럽게 복원
		if (FRotator* SavedRot = SavedControlRotations.Find(PC))
		{
			FControlRotationLerpState LerpState;
			LerpState.StartRotation = PC->GetControlRotation();
			LerpState.TargetRotation = *SavedRot;
			LerpState.Elapsed = 0.f;
			LerpState.Duration = ControlRotationBlendTime;
			LerpState.bIsLerping = true;
			ControlRotationLerpStates.Add(PC, LerpState);

			SavedControlRotations.Remove(PC);
		}

		PC->SetIgnoreLookInput(false);
	}

	const bool bWasLocalTrigger = (LocalTriggerCharacters.Remove(Character) > 0);
	LocalTriggerCharacters.Remove(nullptr);

	// ── 스플릿 스크린 복원 (우리 화면을 바꾼 플레이어가 모두 나갔을 때) ──
	if (bUseSplitScreenTransition && bWasLocalTrigger && LocalTriggerCharacters.Num() == 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: All players left → Split Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: %s exited → Restore 3rd-person camera"), *Character->GetName());
}
