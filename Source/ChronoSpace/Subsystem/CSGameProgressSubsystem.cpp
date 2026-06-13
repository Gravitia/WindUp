// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSGameProgressSubsystem.h"
#include "Save/CSSaveGame.h"
#include "Settings/CSStageDataSettings.h"
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
    const UCSStageDataSettings* Data = UCSStageDataSettings::Get();
    if (!Data) return false;

    // The very first stage of the game is always available.
    int32 FirstChapter, FirstStage;
    if (Data->GetFirstStage(FirstChapter, FirstStage)
        && Chapter == FirstChapter && Stage == FirstStage)
    {
        return true;
    }

    // Any other stage is unlocked once the stage that leads into it is cleared.
    // Across a chapter boundary, the "previous" stage is the prior chapter's last
    // stage, so clearing a chapter's finale unlocks the next chapter automatically.
    int32 PrevChapter, PrevStage;
    if (!Data->GetPrevStage(Chapter, Stage, PrevChapter, PrevStage))
    {
        return false;
    }
    return IsStageCleared(PrevChapter, PrevStage);
}

bool UCSGameProgressSubsystem::GetNextStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const
{
    OutChapter = Chapter;
    OutStage = Stage;

    const UCSStageDataSettings* Data = UCSStageDataSettings::Get();
    return Data && Data->GetNextStage(Chapter, Stage, OutChapter, OutStage);
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
