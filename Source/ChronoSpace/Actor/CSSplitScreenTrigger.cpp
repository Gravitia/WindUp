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
	PlayersInTrigger = 0;
}

void ACSSplitScreenTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger++;

	// 풀스크린 전환 대상 플레이어 인덱스 결정
	int32 TargetPlayerIndex = FixedFullScreenPlayerIndex;

	if (bFullScreenForEnteringPlayer)
	{
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (LP)
		{
			TargetPlayerIndex = LP->GetControllerId();
		}
	}

	// Subsystem을 통해 전환 요청
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			Subsystem->TransitionToFullScreen(TargetPlayerIndex);
			UE_LOG(LogCS, Log, TEXT("SplitScreenTrigger: Player %d entered → Full Screen transition"), TargetPlayerIndex);
		}
	}
}

void ACSSplitScreenTrigger::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	// 모든 플레이어가 트리거를 벗어나면 스플릿 스크린 복원
	if (PlayersInTrigger <= 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCSSplitScreenSubsystem* Subsystem = GI->GetSubsystem<UCSSplitScreenSubsystem>())
			{
				Subsystem->TransitionToSplitScreen();
				UE_LOG(LogCS, Log, TEXT("SplitScreenTrigger: All players left → Split Screen transition"));
			}
		}
	}
}
