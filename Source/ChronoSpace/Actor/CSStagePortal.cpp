// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSStagePortal.h"
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
#include "Subsystem/CSGameProgressSubsystem.h"
#include "UI/CSViewFamilyViewportClient.h"

ACSStagePortal::ACSStagePortal()
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
}

void ACSStagePortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACSStagePortal, PlayerCount);
}

void ACSStagePortal::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSStagePortal::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSStagePortal::OnOverlapEnd);

    ApplyLightVisibility();
}

void ACSStagePortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
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

    if (PlayerCount >= RequiredPlayerCount)
    {
        bTransitionStarted = true;
        MulticastStartTransition();

        FTimerHandle TravelTimer;
        GetWorldTimerManager().SetTimer(TravelTimer, this,
            &ACSStagePortal::RecordProgressAndTravel,
            FMath::Max(FadeDuration, 0.01f), false);
    }
}

void ACSStagePortal::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority() || bTransitionStarted) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    PlayersInTrigger.Remove(Character);
    PlayerCount = PlayersInTrigger.Num();
    ApplyLightVisibility();
}

void ACSStagePortal::OnRep_PlayerCount()
{
    ApplyLightVisibility();
    if (PlayerCount >= 1)
    {
        PreloadTargetLevel();
    }
}

void ACSStagePortal::ApplyLightVisibility()
{
    Light1->SetVisibility(PlayerCount >= 1);
    Light2->SetVisibility(PlayerCount >= 2);
}

void ACSStagePortal::MulticastStartTransition_Implementation()
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

void ACSStagePortal::RecordProgressAndTravel()
{
    if (!HasAuthority()) return;

    if (UCSGameProgressSubsystem* Progress = GetProgressSubsystem())
    {
        if (bMarkCurrentStageCleared)
        {
            Progress->MarkStageCleared(CurrentChapter, CurrentStage);
        }
        if (bUpdateLastPlayed)
        {
            Progress->SetLastPlayedStage(NextChapter, NextStage);
        }
    }

    if (TargetLevelName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ACSStagePortal: TargetLevelName is empty, skipping travel"));
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->ServerTravel(TargetLevelName, false);
    }
}

void ACSStagePortal::PreloadTargetLevel()
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

UCSGameProgressSubsystem* ACSStagePortal::GetProgressSubsystem() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UCSGameProgressSubsystem>();
    }
    return nullptr;
}
