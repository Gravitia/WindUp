// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSSplitScreenTrigger.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "ChronoSpace.h"

ACSSplitScreenTrigger::ACSSplitScreenTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSSplitScreenTrigger::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSSplitScreenTrigger::OnTriggerEndOverlap);
}

void ACSSplitScreenTrigger::BeginPlay()
{
	Super::BeginPlay();
	OccupantOverlapCounts.Empty();
}

void ACSSplitScreenTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	// 화면 전환은 이 머신의 뷰에만 적용된다 - 내 화면을 바꿔야 하는 캐릭터인지 먼저 판단한다.
	if (!UCSSplitScreenSubsystem::ShouldLocalViewRespondTo(Character, bFullScreenForEnteringPlayer, FixedFullScreenPlayerIndex))
	{
		return;
	}

	// 한 캐릭터가 여러 콜리전 컴포넌트로 겹치면 Begin/End 가 그 수만큼 온다
	// (루트 캡슐 + Trigger(OverlapAll) + 중력코어 활성 시 GravityCoreSphere).
	// 겹침 수를 세지 않으면 컴포넌트 하나만 빠져나가도 풀스크린이 풀린다.
	int32& OverlapCount = OccupantOverlapCounts.FindOrAdd(Character);
	if (OverlapCount++ > 0)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			Subsystem->RequestFullScreen(this);
			UE_LOG(LogCS, Log, TEXT("SplitScreenTrigger: local player entered -> Full Screen transition"));
		}
	}
}

void ACSSplitScreenTrigger::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	int32* OverlapCount = OccupantOverlapCounts.Find(Character);
	if (!OverlapCount)
	{
		return;	// 우리 화면을 바꾸지 않았던 캐릭터
	}

	// 아직 다른 콜리전 컴포넌트가 겹쳐 있으면 실제 이탈이 아니다
	if (--(*OverlapCount) > 0)
	{
		return;
	}
	OccupantOverlapCounts.Remove(Character);

	// 파괴된 캐릭터(사망 등)의 약참조는 Remove(nullptr) 로 지워지지 않는다 - 직접 제거한다
	for (auto PurgeIt = OccupantOverlapCounts.CreateIterator(); PurgeIt; ++PurgeIt)
	{
		if (!PurgeIt->Key.IsValid())
		{
			PurgeIt.RemoveCurrent();
		}
	}

	// 우리 화면을 풀스크린으로 만든 캐릭터가 모두 나가면 복원
	if (OccupantOverlapCounts.Num() == 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->ReleaseFullScreen(this);
				UE_LOG(LogCS, Log, TEXT("SplitScreenTrigger: All players left → Split Screen transition"));
			}
		}
	}
}
