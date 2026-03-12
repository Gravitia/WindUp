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
	PrimaryActorTick.bCanEverTick = false;

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
	PlayersInTrigger = 0;
}

void ACSFixedCameraVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// ── 카메라 전환 (로컬 플레이어만) ──
	if (Character->IsLocallyControlled())
	{
		PC->SetViewTargetWithBlend(this, BlendTime);

		// 컨트롤러 회전 저장 후 고정
		SavedControlRotations.Add(PC, PC->GetControlRotation());
		PC->SetControlRotation(FixedControlRotation);
		PC->SetIgnoreLookInput(true);
	}

	// ── 스플릿 스크린 전환 (옵션) ──
	if (bUseSplitScreenTransition)
	{
		int32 TargetPlayerIndex = FixedFullScreenPlayerIndex;

		if (bFullScreenForEnteringPlayer)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				TargetPlayerIndex = LP->GetControllerId();
			}
		}

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->TransitionToFullScreen(TargetPlayerIndex);
				UE_LOG(LogCS, Log, TEXT("FixedCameraVolume: Player %d → Full Screen transition"), TargetPlayerIndex);
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
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// ── 카메라 복원 (로컬 플레이어만) ──
	if (Character->IsLocallyControlled())
	{
		PC->SetViewTargetWithBlend(Character, BlendTime);

		if (FRotator* SavedRot = SavedControlRotations.Find(PC))
		{
			PC->SetControlRotation(*SavedRot);
			SavedControlRotations.Remove(PC);
		}

		PC->SetIgnoreLookInput(false);
	}

	// ── 스플릿 스크린 복원 (모든 플레이어가 나갔을 때) ──
	if (bUseSplitScreenTransition && PlayersInTrigger <= 0)
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
