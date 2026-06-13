// Fill out your copyright notice in the Description page of Project Settings.


#include "Save/CSProgressTestPreset.h"
#include "Save/CSSaveGame.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR

void UCSProgressTestPreset::ApplyToSaveSlot()
{
    UCSSaveGame* Save = Cast<UCSSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UCSSaveGame::StaticClass()));
    if (!Save)
    {
        UE_LOG(LogTemp, Error, TEXT("CSProgressTestPreset: failed to create save object"));
        return;
    }

    // Mark every stage up to and including (ClearedUpToChapter, ClearedUpToStage).
    for (int32 Chapter = 1; Chapter <= ClearedUpToChapter; ++Chapter)
    {
        int32 LastStage;
        if (Chapter < ClearedUpToChapter)
        {
            // Earlier chapters are fully cleared — need their stage count.
            const int32* Count = StagesPerChapter.Find(Chapter);
            if (!Count)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CSProgressTestPreset: StagesPerChapter has no entry for chapter %d; skipping it"),
                    Chapter);
                continue;
            }
            LastStage = *Count;
        }
        else
        {
            // Final chapter is cleared only up to the cutoff stage.
            LastStage = ClearedUpToStage;
        }

        for (int32 Stage = 1; Stage <= LastStage; ++Stage)
        {
            Save->FindOrAddRecord(Chapter, Stage).bCleared = true;
        }
    }

    // Resolve last-played: explicit if set, otherwise the stage after the last cleared one.
    if (LastPlayedChapter > 0 && LastPlayedStage > 0)
    {
        Save->LastPlayedChapter = LastPlayedChapter;
        Save->LastPlayedStage = LastPlayedStage;
    }
    else if (ClearedUpToStage > 0)
    {
        Save->LastPlayedChapter = ClearedUpToChapter;
        Save->LastPlayedStage = ClearedUpToStage + 1;
    }
    else
    {
        Save->LastPlayedChapter = ClearedUpToChapter;
        Save->LastPlayedStage = 1;
    }

    if (UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, UserIndex))
    {
        UE_LOG(LogTemp, Log,
            TEXT("CSProgressTestPreset: wrote slot '%s' (%d stage records, last played C%d_S%d)"),
            *SaveSlotName, Save->StageRecords.Num(),
            Save->LastPlayedChapter, Save->LastPlayedStage);
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("CSProgressTestPreset: failed to write slot '%s'"), *SaveSlotName);
    }
}

void UCSProgressTestPreset::DeleteSaveSlot()
{
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
    {
        UGameplayStatics::DeleteGameInSlot(SaveSlotName, UserIndex);
        UE_LOG(LogTemp, Log, TEXT("CSProgressTestPreset: deleted slot '%s'"), *SaveSlotName);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CSProgressTestPreset: slot '%s' does not exist"), *SaveSlotName);
    }
}

#endif // WITH_EDITOR
