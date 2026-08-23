// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSLabyrinthKeyActivator.h"
#include "EngineUtils.h"
#include "Actor/CSLabyrinthKey.h"
#include "ChronoSpace.h"

// Sets default values
ACSLabyrinthKeyActivator::ACSLabyrinthKeyActivator()
{
	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	MaxKeyCount = 10;
}

// Called when the game starts or when spawned
void ACSLabyrinthKeyActivator::BeginPlay()
{
	Super::BeginPlay();
	
	if ( HasAuthority() )
	{
		SetLabyrinthKey();
	}
}
void ACSLabyrinthKeyActivator::SetLabyrinthKey()
{
	for (TActorIterator<ACSLabyrinthKey> It(GetWorld()); It; ++It)
	{
		ACSLabyrinthKey* LabyrinthKey = *It;

		if ( LabyrinthKey )
		{
			Keys.Emplace(LabyrinthKey);
		}
	}

	// 아직 꺼져 있는 키만 후보로 모은다. (이미 켜진 키를 "충돌 켜짐"으로 판별하던 방식은
	// 레벨에 충돌이 켜진 키가 하나라도 있으면 while 이 영원히 끝나지 않았다.)
	TArray<ACSLabyrinthKey*> Candidates;
	for (ACSLabyrinthKey* Key : Keys)
	{
		if (IsValid(Key) && !Key->bIsActive)
		{
			Candidates.Add(Key);
		}
	}

	// 키가 부족하면 있는 만큼만 켠다. (예전엔 조기 return 으로 하나도 안 켜져 퍼즐이 풀 수 없었다.)
	const int32 ToActivate = FMath::Min(MaxKeyCount, Candidates.Num());
	if (Candidates.Num() < MaxKeyCount)
	{
		UE_LOG(LogCS, Warning, TEXT("CSLabyrinthKeyActivator: keys available %d < MaxKeyCount %d - activating all available"), Candidates.Num(), MaxKeyCount);
	}

	// 부분 Fisher-Yates 셔플: 앞에서 ToActivate 개를 무작위로 고른다. 종료 보장.
	for (int32 i = 0; i < ToActivate; ++i)
	{
		const int32 SwapIdx = FMath::RandRange(i, Candidates.Num() - 1);
		Candidates.Swap(i, SwapIdx);
		Candidates[i]->SetActive(true);
	}
}


