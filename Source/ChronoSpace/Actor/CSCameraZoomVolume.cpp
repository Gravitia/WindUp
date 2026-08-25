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
	Occupants.Empty();
}

void ACSCameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	// 한 캐릭터가 여러 콜리전 컴포넌트로 겹치면 Begin/End 가 그 수만큼 온다
	// (루트 캡슐 + Trigger(OverlapAll) + 중력코어 활성 시 GravityCoreSphere).
	// 캐릭터 단위로 겹침 수를 세서 첫 진입 / 마지막 이탈에서만 처리한다.
	FCSZoomOccupant& Occupant = Occupants.FindOrAdd(Player);
	if (Occupant.OverlapCount++ > 0)
	{
		return;
	}

	// ── 줌 적용 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(ZoomLength, ZoomSpeed);
	}

	// ── 스플릿 스크린 전환 (옵션) ── 이 머신의 화면만 바뀌므로 로컬 판정이 필요하다
	if (bUseSplitScreenTransition
		&& UCSSplitScreenSubsystem::ShouldLocalViewRespondTo(Player, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		Occupant.bRequestedFullScreen = true;

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

	FCSZoomOccupant* Occupant = Occupants.Find(Player);
	if (!Occupant) return;

	// 아직 다른 콜리전 컴포넌트가 겹쳐 있으면 실제 이탈이 아니다
	if (--Occupant->OverlapCount > 0)
	{
		return;
	}

	// ── 줌 복원 (로컬 플레이어만) ──
	if (Player->IsLocallyControlled())
	{
		Player->ZoomCamera(0.f, ZoomSpeed);
	}

	const bool bWasLocalTrigger = Occupant->bRequestedFullScreen;
	Occupants.Remove(Player);

	// 파괴된 캐릭터(사망 등)의 약참조는 Remove(nullptr) 로 지워지지 않는다 - 직접 제거한다
	for (auto PurgeIt = Occupants.CreateIterator(); PurgeIt; ++PurgeIt)
	{
		if (!PurgeIt->Key.IsValid())
		{
			PurgeIt.RemoveCurrent();
		}
	}

	bool bAnyRequesterLeft = false;
	for (const auto& Pair : Occupants)
	{
		if (Pair.Value.bRequestedFullScreen) { bAnyRequesterLeft = true; break; }
	}

	// ── 스플릿 스크린 복원 (우리 화면을 바꾼 플레이어가 모두 나갔을 때) ──
	if (bUseSplitScreenTransition && bWasLocalTrigger && !bAnyRequesterLeft)
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
