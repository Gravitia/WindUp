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
	Occupants.Empty();
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

	// 한 캐릭터가 여러 콜리전 컴포넌트로 겹치면 Begin/End 가 그 수만큼 온다
	// (루트 캡슐 + Trigger(OverlapAll) + 중력코어 활성 시 GravityCoreSphere).
	// SetIgnoreLookInput 은 스택 카운터라 진입/해제 횟수가 맞지 않으면 시점 입력이 영구히 잠긴다.
	// 그래서 캐릭터 단위로 겹침 수를 세고, 첫 진입/마지막 이탈에서만 실제 처리를 한다.
	FCSFixedCameraOccupant& Occupant = Occupants.FindOrAdd(Character);
	if (Occupant.OverlapCount++ > 0)
	{
		return;
	}

	// ── 카메라 전환 (로컬 플레이어만) ──
	if (PC && Character->IsLocallyControlled())
	{
		// 어떤 PC 를 잠갔는지 기억한다 - 사망/언포제스로 EndOverlap 시점에
		// Character->GetController() 가 null 이어도 잠금을 풀 수 있어야 한다.
		Occupant.LockedPC = PC;
		Occupant.bLockedInput = true;

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
		Occupant.bRequestedFullScreen = true;

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->RequestFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: local player entered -> Full Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: %s entered -> Fixed camera, ControlRotation locked"), *Character->GetName());
}

void ACSFixedCameraVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	FCSFixedCameraOccupant* Occupant = Occupants.Find(Character);
	if (!Occupant)
	{
		return;	// 우리가 진입 처리를 하지 않은 캐릭터
	}

	// 아직 다른 콜리전 컴포넌트가 겹쳐 있으면 실제 이탈이 아니다
	if (--Occupant->OverlapCount > 0)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		PC = Occupant->LockedPC.Get();	// 컨트롤러가 이미 떨어졌어도 잠금은 풀어야 한다
	}

	// ── 카메라 복원 (진입 때 우리가 잠근 경우) ──
	if (PC && Occupant->bLockedInput)
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

		// 진입에서 건 잠금과 정확히 1:1
		PC->SetIgnoreLookInput(false);
	}

	const bool bWasFullScreenRequester = Occupant->bRequestedFullScreen;
	Occupants.Remove(Character);

	// 파괴된 캐릭터 항목 정리 (약참조는 Remove(nullptr) 로 지워지지 않는다)
	for (auto It = Occupants.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// ── 스플릿 스크린 복원 (우리 화면을 바꾼 플레이어가 모두 나갔을 때) ──
	bool bAnyRequesterLeft = false;
	for (const auto& Pair : Occupants)
	{
		if (Pair.Value.bRequestedFullScreen)
		{
			bAnyRequesterLeft = true;
			break;
		}
	}

	if (bUseSplitScreenTransition && bWasFullScreenRequester && !bAnyRequesterLeft)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->ReleaseFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: All players left -> Split Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: %s exited -> Restore 3rd-person camera"), *Character->GetName());
}
