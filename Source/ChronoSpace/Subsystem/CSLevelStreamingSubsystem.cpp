// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Game/CSGameState.h"
#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"

UCSLevelStreamingSubsystem::UCSLevelStreamingSubsystem()
{
    CurrentLevelIndex = 0;
    CurrentStreamingLevel = nullptr;
    CurrentChapter = 1;
    CurrentStage = 1;
}

void UCSLevelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem initialized"));
}

void UCSLevelStreamingSubsystem::Deinitialize()
{
    if (CurrentStreamingLevel)
    {
        UnloadCurrentLevel();
    }

    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem deinitialized"));
}

bool UCSLevelStreamingSubsystem::StreamLevel(int32 ChapterNumber, int32 StageNumber)
{
    /*
    // Only host/server can stream levels in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot stream levels"));
        return false;
    }

    // Calculate level index from chapter and stage
    int32 LevelIndex = CalculateLevelIndex(ChapterNumber, StageNumber);

    if (!LevelAssets.IsValidIndex(LevelIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid level for C%d_S%d (Index: %d)"), ChapterNumber, StageNumber, LevelIndex);
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid world for level streaming"));
        return false;
    }

    FString LevelPath = LevelAssets[LevelIndex].GetLongPackageName();

    CurrentStreamingLevel = ULevelStreamingDynamic::LoadLevelInstance(
        World,
        LevelPath,
        FVector::ZeroVector,
        FRotator::ZeroRotator
    );

    if (CurrentStreamingLevel)
    {
        CurrentChapter = ChapterNumber;
        CurrentStage = StageNumber;
        CurrentLevelIndex = LevelIndex;

        // Update progress subsystem with current stage
        UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
        if (ProgressSubsystem)
        {
            ProgressSubsystem->SetLastPlayedStage(ChapterNumber, StageNumber);
        }

        OnLevelStreamed.Broadcast(ChapterNumber, StageNumber);
        UE_LOG(LogTemp, Log, TEXT("Level streamed successfully: C%d_S%d (Index: %d)"), ChapterNumber, StageNumber, LevelIndex);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to stream level: C%d_S%d (Index: %d)"), ChapterNumber, StageNumber, LevelIndex);
        return false;
    }
    */
}

bool UCSLevelStreamingSubsystem::UnloadCurrentLevel()
{
    // Only host/server can unload levels in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot unload levels"));
        return false;
    }

    if (!CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("No current level to unload"));
        return false;
    }

    int32 UnloadedChapter = CurrentChapter;
    int32 UnloadedStage = CurrentStage;

    CurrentStreamingLevel->SetShouldBeLoaded(false);
    CurrentStreamingLevel->SetShouldBeVisible(false);
    CurrentStreamingLevel = nullptr;

    OnLevelUnloaded.Broadcast(UnloadedChapter, UnloadedStage);
    UE_LOG(LogTemp, Log, TEXT("Level unloaded successfully: C%d_S%d"), UnloadedChapter, UnloadedStage);

    return true;
}

FVector UCSLevelStreamingSubsystem::GetSpawnPosition() const
{
    if (SpawnPositions.IsValidIndex(CurrentLevelIndex))
    {
        return SpawnPositions[CurrentLevelIndex];
    }

    UE_LOG(LogTemp, Warning, TEXT("No spawn position for C%d_S%d (Index: %d)"), CurrentChapter, CurrentStage, CurrentLevelIndex);
    return FVector::ZeroVector;
}

void UCSLevelStreamingSubsystem::SetLevelData(const TArray<TSoftObjectPtr<UWorld>>& InLevelAssets, const TArray<FVector>& InSpawnPositions)
{
    LevelAssets = InLevelAssets;
    SpawnPositions = InSpawnPositions;
    UE_LOG(LogTemp, Log, TEXT("Level data set: %d levels, %d spawn positions"), LevelAssets.Num(), SpawnPositions.Num());
}

void UCSLevelStreamingSubsystem::CompleteCurrentStage()
{
    // Only host/server can complete stages in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot complete stages"));
        return;
    }

    UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
    if (ProgressSubsystem)
    {
        ProgressSubsystem->ClearStage(CurrentChapter, CurrentStage);
        UE_LOG(LogTemp, Log, TEXT("Stage completed: C%d_S%d"), CurrentChapter, CurrentStage);
    }
}

void UCSLevelStreamingSubsystem::GetCurrentStage(int32& OutChapter, int32& OutStage) const
{
    OutChapter = CurrentChapter;
    OutStage = CurrentStage;
}

bool UCSLevelStreamingSubsystem::IsLevelStreaming() const
{
    return CurrentStreamingLevel != nullptr;
}

int32 UCSLevelStreamingSubsystem::CalculateLevelIndex(int32 ChapterNumber, int32 StageNumber) const
{
    // Assuming 10 stages per chapter, starting from Chapter 1, Stage 1
    // C1_S1 = Index 0, C1_S2 = Index 1, ... C2_S1 = Index 10, etc.
    return (ChapterNumber - 1) * 10 + (StageNumber - 1);
}

bool UCSLevelStreamingSubsystem::IsMultiplayerClient() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetNetMode() == NM_Client;
    }
    return false;
}

UCSGameProgressSubsystem* UCSLevelStreamingSubsystem::GetProgressSubsystem() const
{
    return GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
}

ACSGameState* UCSLevelStreamingSubsystem::GetCSGameState() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return Cast<ACSGameState>(World->GetGameState());
    }
    return nullptr;
}
