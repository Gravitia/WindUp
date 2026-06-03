// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSGameProgressSubsystem.h"
#include "Save/CSSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UCSGameProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (DoesSaveExist())
    {
        LoadGame();
    }
    else
    {
        CreateNewSaveGameObject();
    }

    UE_LOG(LogTemp, Log, TEXT("CSGameProgressSubsystem initialized"));
}

void UCSGameProgressSubsystem::Deinitialize()
{
    if (CurrentSaveGame && !IsClient())
    {
        SaveGame();
    }
    Super::Deinitialize();
}

bool UCSGameProgressSubsystem::SaveGame()
{
    if (!CurrentSaveGame) return false;
    if (IsClient()) return false;

    const bool bOk = UGameplayStatics::SaveGameToSlot(CurrentSaveGame, SaveSlotName, UserIndex);
    if (bOk)
    {
        OnGameSaved.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("CSGameProgress: saved to slot '%s'"), *SaveSlotName);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CSGameProgress: failed to save slot '%s'"), *SaveSlotName);
    }
    return bOk;
}

bool UCSGameProgressSubsystem::LoadGame()
{
    if (!DoesSaveExist()) return false;

    USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SaveSlotName, UserIndex);
    UCSSaveGame* Cast = ::Cast<UCSSaveGame>(Loaded);

    if (!Cast)
    {
        UE_LOG(LogTemp, Error, TEXT("CSGameProgress: load failed (corrupt or version mismatch)"));
        return false;
    }

    CurrentSaveGame = Cast;
    OnGameLoaded.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("CSGameProgress: loaded slot '%s' (version %d)"),
        *SaveSlotName, CurrentSaveGame->SaveVersion);
    return true;
}

bool UCSGameProgressSubsystem::NewGame()
{
    if (IsClient()) return false;

    CreateNewSaveGameObject();
    const bool bOk = SaveGame();
    OnNewGameStarted.Broadcast();
    return bOk;
}

bool UCSGameProgressSubsystem::DoesSaveExist() const
{
    return UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex);
}

void UCSGameProgressSubsystem::MarkStageCleared(int32 Chapter, int32 Stage)
{
    if (!CurrentSaveGame || IsClient()) return;

    FStageRecord& Record = CurrentSaveGame->FindOrAddRecord(Chapter, Stage);
    const bool bWasNew = !Record.bCleared;
    Record.bCleared = true;

    if (bWasNew)
    {
        OnStageCleared.Broadcast(Chapter, Stage);
        UE_LOG(LogTemp, Log, TEXT("CSGameProgress: stage cleared C%d_S%d"), Chapter, Stage);
    }

    SaveGame();
}

bool UCSGameProgressSubsystem::IsStageCleared(int32 Chapter, int32 Stage) const
{
    if (!CurrentSaveGame) return false;
    const FStageRecord* Record = CurrentSaveGame->FindRecord(Chapter, Stage);
    return Record && Record->bCleared;
}

bool UCSGameProgressSubsystem::IsStageUnlocked(int32 Chapter, int32 Stage) const
{
    if (Chapter <= 1 && Stage <= 1) return true;

    if (Stage > 1)
    {
        return IsStageCleared(Chapter, Stage - 1);
    }

    // First stage of a later chapter — unlocked when previous chapter's last stage is cleared.
    const int32 PrevChapter = Chapter - 1;
    const int32* PrevLastStage = StagesPerChapter.Find(PrevChapter);
    if (!PrevLastStage)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CSGameProgress: StagesPerChapter has no entry for chapter %d"), PrevChapter);
        return false;
    }
    return IsStageCleared(PrevChapter, *PrevLastStage);
}

void UCSGameProgressSubsystem::SetLastPlayedStage(int32 Chapter, int32 Stage)
{
    if (!CurrentSaveGame || IsClient()) return;

    CurrentSaveGame->LastPlayedChapter = Chapter;
    CurrentSaveGame->LastPlayedStage = Stage;
    SaveGame();
}

void UCSGameProgressSubsystem::GetLastPlayedStage(int32& OutChapter, int32& OutStage) const
{
    if (CurrentSaveGame)
    {
        OutChapter = CurrentSaveGame->LastPlayedChapter;
        OutStage   = CurrentSaveGame->LastPlayedStage;
    }
    else
    {
        OutChapter = 1;
        OutStage   = 1;
    }
}

void UCSGameProgressSubsystem::IncrementTotalDeaths()
{
    if (!CurrentSaveGame || IsClient()) return;
    CurrentSaveGame->TotalDeaths++;
    SaveGame();
}

void UCSGameProgressSubsystem::IncrementStageDeath(int32 Chapter, int32 Stage)
{
    if (!CurrentSaveGame || IsClient()) return;
    FStageRecord& Record = CurrentSaveGame->FindOrAddRecord(Chapter, Stage);
    Record.DeathCount++;
    CurrentSaveGame->TotalDeaths++;
    SaveGame();
}

bool UCSGameProgressSubsystem::IsClient() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() == NM_Client;
}

void UCSGameProgressSubsystem::CreateNewSaveGameObject()
{
    CurrentSaveGame = Cast<UCSSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCSSaveGame::StaticClass()));

    if (!CurrentSaveGame)
    {
        UE_LOG(LogTemp, Error, TEXT("CSGameProgress: failed to create save game object"));
    }
}
