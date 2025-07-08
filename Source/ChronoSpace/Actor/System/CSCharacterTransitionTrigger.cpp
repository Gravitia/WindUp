// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCharacterTransitionTrigger.h"
#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

ACSCharacterTransitionTrigger::ACSCharacterTransitionTrigger()
{
    PrimaryActorTick.bCanEverTick = false;
    bCompleteStageOnTransition = true;

    GetCollisionComponent()->OnComponentBeginOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapBegin);
    GetCollisionComponent()->OnComponentEndOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapEnd);

    bReplicates = true;
}

void ACSCharacterTransitionTrigger::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("CSCharacterTransitionTrigger initialized"));

}

void ACSCharacterTransitionTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character || PlayersInTrigger.Contains(Character))
    {
        return;
    }

    PlayersInTrigger.Add(Character);
    UE_LOG(LogTemp, Log, TEXT("Player entered transition trigger: %d/2 players"), PlayersInTrigger.Num());

    CheckAllPlayersInTrigger();
}

void ACSCharacterTransitionTrigger::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character)
    {
        PlayersInTrigger.Remove(Character);
        UE_LOG(LogTemp, Log, TEXT("Player left transition trigger: %d/2 players"), PlayersInTrigger.Num());
    }
}

void ACSCharacterTransitionTrigger::CheckAllPlayersInTrigger()
{
    if (PlayersInTrigger.Num() >= 2 && HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("All players in trigger - starting transition"));
        MoveCharactersMulticast();
    }
}

void ACSCharacterTransitionTrigger::MoveCharactersMulticast_Implementation()
{
    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get CSLevelStreamingSubsystem"));
        return;
    }

    // Move all characters to new position
    for (ACharacter* Character : PlayersInTrigger)
    {
        if (Character)
        {
            MoveCharacterToPosition(Character);
        }
    }

    // Complete stage if enabled
    if (bCompleteStageOnTransition)
    {
        LevelSubsystem->CompleteCurrentStage();
    }

    // Unload current level
    LevelSubsystem->UnloadCurrentLevel();

    PlayersInTrigger.Empty();
    UE_LOG(LogTemp, Log, TEXT("Character transition completed"));
}

void ACSCharacterTransitionTrigger::MoveCharacterToPosition(ACharacter* Character)
{
    if (!Character)
    {
        return;
    }

    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (LevelSubsystem)
    {
        FVector NewPosition = LevelSubsystem->GetSpawnPosition();
        Character->SetActorLocation(NewPosition);
        UE_LOG(LogTemp, Log, TEXT("Character moved to position: %s"), *NewPosition.ToString());
    }
}

UCSLevelStreamingSubsystem* ACSCharacterTransitionTrigger::GetLevelStreamingSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSLevelStreamingSubsystem>();
    }
    return nullptr;
}

UCSGameProgressSubsystem* ACSCharacterTransitionTrigger::GetProgressSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
    }
    return nullptr;
}