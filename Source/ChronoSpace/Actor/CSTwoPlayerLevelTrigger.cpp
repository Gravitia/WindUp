// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSTwoPlayerLevelTrigger.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "Net/UnrealNetwork.h"
#include "UI/CSViewFamilyViewportClient.h"

ACSTwoPlayerLevelTrigger::ACSTwoPlayerLevelTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Light1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light1"));
	Light1->SetupAttachment(RootComponent);
	Light1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light1->SetVisibility(false);

	Light2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light2"));
	Light2->SetupAttachment(RootComponent);
	Light2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light2->SetVisibility(false);

	TargetLevelName = TEXT("");
	FadeDuration = 1.0f;
	PlayerCount = 0;
	bTransitionStarted = false;
	bPreloadStarted = false;
}

void ACSTwoPlayerLevelTrigger::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACSTwoPlayerLevelTrigger, PlayerCount);
}

void ACSTwoPlayerLevelTrigger::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSTwoPlayerLevelTrigger::OnOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSTwoPlayerLevelTrigger::OnOverlapEnd);

	ApplyLightVisibility();
}

void ACSTwoPlayerLevelTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bTransitionStarted) return;

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsPlayerControlled()) return;
	if (PlayersInTrigger.Contains(Character)) return;

	PlayersInTrigger.Add(Character);
	PlayerCount = PlayersInTrigger.Num();
	ApplyLightVisibility();

	if (PlayerCount == 1)
	{
		PreloadTargetLevel();
	}

	if (PlayerCount >= 2)
	{
		bTransitionStarted = true;
		MulticastStartTransition();

		FTimerHandle TravelTimer;
		GetWorldTimerManager().SetTimer(TravelTimer, this,
			&ACSTwoPlayerLevelTrigger::ExecuteServerTravel, FMath::Max(FadeDuration, 0.01f), false);
	}
}

void ACSTwoPlayerLevelTrigger::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || bTransitionStarted) return;

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	PlayersInTrigger.Remove(Character);
	PlayerCount = PlayersInTrigger.Num();
	ApplyLightVisibility();
}

void ACSTwoPlayerLevelTrigger::OnRep_PlayerCount()
{
	ApplyLightVisibility();
	if (PlayerCount >= 1)
	{
		PreloadTargetLevel();
	}
}

void ACSTwoPlayerLevelTrigger::ApplyLightVisibility()
{
	Light1->SetVisibility(PlayerCount >= 1);
	Light2->SetVisibility(PlayerCount >= 2);
}

void ACSTwoPlayerLevelTrigger::MulticastStartTransition_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->DisableInput(PC);
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UCSViewFamilyViewportClient* VC = Cast<UCSViewFamilyViewportClient>(GI->GetGameViewportClient()))
		{
			VC->StartFade(1.0f, FadeDuration);
		}
	}
}

void ACSTwoPlayerLevelTrigger::ExecuteServerTravel()
{
	if (!HasAuthority() || TargetLevelName.IsEmpty()) return;

	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(TargetLevelName, false);
	}
}

void ACSTwoPlayerLevelTrigger::PreloadTargetLevel()
{
	if (bPreloadStarted || TargetLevelName.IsEmpty()) return;
	bPreloadStarted = true;

	FString PackageName = TargetLevelName;
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		FString Resolved;
		if (FPackageName::SearchForPackageOnDisk(TargetLevelName, nullptr, &Resolved))
		{
			PackageName = Resolved;
		}
	}

	LoadPackageAsync(PackageName, FLoadPackageAsyncDelegate(), 0, PKG_ContainsMap);
}
