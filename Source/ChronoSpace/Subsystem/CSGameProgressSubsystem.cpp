// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSGameProgressSubsystem.h"
#include "Save/CSSaveGame.h"
#include "Game/CSGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UCSGameProgressSubsystem::UCSGameProgressSubsystem()
{
    CurrentSaveGame = nullptr;
}

void UCSGameProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Try to load existing save game, or create new one
    if (DoesSaveExist())
    {
        LoadGame();
    }
    else
    {
        CreateNewSaveGame();
    }

    UE_LOG(LogTemp, Log, TEXT("CSGameProgressSubsystem initialized"));
}

void UCSGameProgressSubsystem::Deinitialize()
{
    // Auto save before shutdown
    if (CurrentSaveGame)
    {
        SaveGame();
    }

    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("CSGameProgressSubsystem deinitialized"));
}

bool UCSGameProgressSubsystem::SaveGame()
{
    if (!CurrentSaveGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("No save game object to save"));
        return false;
    }

    // Only host/server can save in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot save game data"));
        return false;
    }

    bool bSaveSuccess = UGameplayStatics::SaveGameToSlot(CurrentSaveGame, DefaultSaveSlotName, DefaultUserIndex);

    if (bSaveSuccess)
    {
        OnGameSaved.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("Game saved successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save game"));
    }

    return bSaveSuccess;
}

bool UCSGameProgressSubsystem::LoadGame()
{
    if (!DoesSaveExist())
    {
        UE_LOG(LogTemp, Warning, TEXT("No save file exists"));
        return false;
    }

    USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(DefaultSaveSlotName, DefaultUserIndex);
    CurrentSaveGame = Cast<UCSSaveGame>(LoadedGame);

    if (CurrentSaveGame)
    {
        OnGameLoaded.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("Game loaded successfully"));
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load save game"));
        CreateNewSaveGame(); // Fallback to new game
        return false;
    }
}

bool UCSGameProgressSubsystem::NewGame()
{
    CreateNewSaveGame();
    SaveGame();

    OnNewGameStarted.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("New game started"));

    return true;
}

bool UCSGameProgressSubsystem::DoesSaveExist() const
{
    return UGameplayStatics::DoesSaveGameExist(DefaultSaveSlotName, DefaultUserIndex);
}

void UCSGameProgressSubsystem::ClearStage(int32 ChapterNumber, int32 StageNumber)
{
    if (!CurrentSaveGame)
    {
        UE_LOG(LogTemp, Warning, TEXT("No save game object for stage clear"));
        return;
    }

    // Only host/server can clear stages in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot clear stages"));
        return;
    }

    bool bWasAlreadyCleared = CurrentSaveGame->IsStageCleared(ChapterNumber, StageNumber);

    CurrentSaveGame->ClearStage(ChapterNumber, StageNumber);

    if (!bWasAlreadyCleared)
    {
        OnStageCleared.Broadcast(ChapterNumber, StageNumber);
        UE_LOG(LogTemp, Log, TEXT("Stage cleared: C%d_S%d"), ChapterNumber, StageNumber);
    }

    // Auto save after stage clear
    SaveGame();
}

bool UCSGameProgressSubsystem::IsStageCleared(int32 ChapterNumber, int32 StageNumber) const
{
    return CurrentSaveGame ? CurrentSaveGame->IsStageCleared(ChapterNumber, StageNumber) : false;
}

bool UCSGameProgressSubsystem::IsStageUnlocked(int32 ChapterNumber, int32 StageNumber) const
{
    return CurrentSaveGame ? CurrentSaveGame->IsStageUnlocked(ChapterNumber, StageNumber) : false;
}

void UCSGameProgressSubsystem::SetLastPlayedStage(int32 ChapterNumber, int32 StageNumber)
{
    if (CurrentSaveGame)
    {
        CurrentSaveGame->SetLastPlayedStage(ChapterNumber, StageNumber);
        SaveGame(); // Auto save continue position
    }
}

void UCSGameProgressSubsystem::GetLastPlayedStage(int32& OutChapter, int32& OutStage) const
{
    if (CurrentSaveGame)
    {
        CurrentSaveGame->GetLastPlayedStage(OutChapter, OutStage);
    }
    else
    {
        OutChapter = 1;
        OutStage = 1;
    }
}

void UCSGameProgressSubsystem::IncrementDeathCount()
{
    if (CurrentSaveGame)
    {
        CurrentSaveGame->IncrementDeathCount();
        SaveGame(); // Auto save death count
    }
}

bool UCSGameProgressSubsystem::LoadGameFromUI()
{
    // Only host can load in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Only host can load game from UI"));
        return false;
    }

    return LoadGame();
}

bool UCSGameProgressSubsystem::NewGameFromUI()
{
    // Only host can start new game in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Only host can start new game from UI"));
        return false;
    }

    return NewGame();
}

void UCSGameProgressSubsystem::CreateNewSaveGame()
{
    CurrentSaveGame = Cast<UCSSaveGame>(UGameplayStatics::CreateSaveGameObject(UCSSaveGame::StaticClass()));

    if (CurrentSaveGame)
    {
        CurrentSaveGame->SaveSlotName = DefaultSaveSlotName;
        UE_LOG(LogTemp, Log, TEXT("New save game created"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create new save game"));
    }
}

bool UCSGameProgressSubsystem::IsMultiplayerClient() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetNetMode() == NM_Client;
    }
    return false;
}

ACSGameState* UCSGameProgressSubsystem::GetCSGameState() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return Cast<ACSGameState>(World->GetGameState());
    }
    return nullptr;
}