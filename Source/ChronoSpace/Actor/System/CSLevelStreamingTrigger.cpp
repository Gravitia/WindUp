// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSLevelStreamingTrigger.h"
#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ACSLevelStreamingTrigger::ACSLevelStreamingTrigger()
{
    PrimaryActorTick.bCanEverTick = false;
    ChapterNumber = 1;
    StageNumber = 1;

    GetCollisionComponent()->OnComponentBeginOverlap.AddDynamic(this, &ACSLevelStreamingTrigger::OnOverlapBegin);

    bReplicates = true;
}

void ACSLevelStreamingTrigger::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingTrigger initialized for C%d_S%d"), ChapterNumber, StageNumber);
}

void ACSLevelStreamingTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsServerPlayer(OtherActor))
    {
        return;
    }

    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get CSLevelStreamingSubsystem"));
        return;
    }

    // Always use chapter/stage based streaming
    if (ChapterNumber > 0 && StageNumber > 0)
    {
        LevelSubsystem->StreamLevel(ChapterNumber, StageNumber);
        UE_LOG(LogTemp, Log, TEXT("Streaming level for stage: C%d_S%d"), ChapterNumber, StageNumber);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid chapter/stage numbers: C%d_S%d"), ChapterNumber, StageNumber);
    }
}

bool ACSLevelStreamingTrigger::IsServerPlayer(AActor* Actor)
{
    ACharacter* Character = Cast<ACharacter>(Actor);
    if (!Character)
    {
        return false;
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC)
    {
        return false;
    }

    // Only server/host players can trigger level streaming
    return HasAuthority() && PC->IsLocalPlayerController();
}

UCSLevelStreamingSubsystem* ACSLevelStreamingTrigger::GetLevelStreamingSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSLevelStreamingSubsystem>();
    }
    return nullptr;
}
