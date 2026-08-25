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
	LocalTriggerCharacters.Empty();
}

void ACSCameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	// ── 줌 적용 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(ZoomLength, ZoomSpeed);
	}

	// ── 스플릿 스크린 전환 (옵션) ── 이 머신의 화면만 바뀌므로 로컬 판정이 필요하다
	if (bUseSplitScreenTransition
		&& UCSSplitScreenSubsystem::ShouldLocalViewRespondTo(Player, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		LocalTriggerCharacters.Add(Player);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->RequestFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: local player entered -> Full Screen transition"));
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

	// ── 줌 복원 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(0.f, ZoomSpeed);
	}

	const bool bWasLocalTrigger = (LocalTriggerCharacters.Remove(Player) > 0);
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
				UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: All players left → Split Screen transition"));
			}
		}
	}

	UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: %s exited → Restore original zoom"), *Player->GetName());
}
