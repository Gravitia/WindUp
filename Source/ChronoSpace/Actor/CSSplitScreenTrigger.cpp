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
	LocalTriggerCharacters.Empty();
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

	// 카운트 대신 집합으로 추적한다 (사망/언포제스로 EndOverlap 에서 판정이 뒤집혀도 드리프트 없음)
	LocalTriggerCharacters.Add(Character);

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

	if (LocalTriggerCharacters.Remove(Character) == 0)
	{
		return;	// 우리 화면을 바꾸지 않았던 캐릭터
	}
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

	// 우리 화면을 풀스크린으로 만든 캐릭터가 모두 나가면 복원
	if (LocalTriggerCharacters.Num() == 0)
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
