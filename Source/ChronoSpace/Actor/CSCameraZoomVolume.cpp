// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSCameraZoomVolume.h"
#include "Components/BoxComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ChronoSpace.h"

ACSCameraZoomVolume::ACSCameraZoomVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCameraZoomVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSCameraZoomVolume::OnTriggerEndOverlap);
}

void ACSCameraZoomVolume::BeginPlay()
{
	Super::BeginPlay();
	PlayersInTrigger = 0;
}

void ACSCameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// ── 줌 적용 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(ZoomLength, ZoomSpeed);
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
				UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: Player %d → Full Screen transition"), TargetPlayerIndex);
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: %s entered → ZoomLength: %.1f"), *Player->GetName(), ZoomLength);
}

void ACSCameraZoomVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// ── 줌 복원 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(0.f, ZoomSpeed);
	}

	// ── 스플릿 스크린 복원 (모든 플레이어가 나갔을 때) ──
	if (bUseSplitScreenTransition && PlayersInTrigger <= 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: All players left → Split Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: %s exited → Restore original zoom"), *Player->GetName());
}
