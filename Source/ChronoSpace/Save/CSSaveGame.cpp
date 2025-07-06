// Fill out your copyright notice in the Description page of Project Settings.


#include "Save/CSSaveGame.h"

UCSSaveGame::UCSSaveGame()
{
    SaveSlotName = TEXT("ChronoSpaceSave");
    LastPlayedChapter = 1;
    LastPlayedStage = 1;
    TotalDeaths = 0;

    InitializeDefaultStages();
}

bool UCSSaveGame::IsStageCleared(int32 ChapterNumber, int32 StageNumber) const
{
    const FStageRecord* Record = FindStageRecord(ChapterNumber, StageNumber);
    return Record ? Record->bStageCleared : false;
}

void UCSSaveGame::ClearStage(int32 ChapterNumber, int32 StageNumber)
{
    EnsureStageRecordExists(ChapterNumber, StageNumber);

    FStageRecord* Record = FindStageRecord(ChapterNumber, StageNumber);
    if (Record)
    {
        Record->bStageCleared = true;
        UE_LOG(LogTemp, Log, TEXT("Stage cleared: C%d_S%d"), ChapterNumber, StageNumber);
    }
}

bool UCSSaveGame::IsStageUnlocked(int32 ChapterNumber, int32 StageNumber) const
{
    // First stage of first chapter is always unlocked
    if (ChapterNumber == 1 && StageNumber == 1)
        return true;

    // Check if previous stage is cleared
    if (StageNumber > 1)
    {
        return IsStageCleared(ChapterNumber, StageNumber - 1);
    }
    else
    {
        // First stage of chapter - check if previous chapter's last stage is cleared
        if (ChapterNumber > 1)
        {
            return IsStageCleared(ChapterNumber - 1, 5); // Assuming 5 stages per chapter
        }
    }

    return false;
}

void UCSSaveGame::SetLastPlayedStage(int32 ChapterNumber, int32 StageNumber)
{
    LastPlayedChapter = ChapterNumber;
    LastPlayedStage = StageNumber;
}

void UCSSaveGame::GetLastPlayedStage(int32& OutChapter, int32& OutStage) const
{
    OutChapter = LastPlayedChapter;
    OutStage = LastPlayedStage;
}

void UCSSaveGame::IncrementDeathCount()
{
    TotalDeaths++;
}

FStageRecord* UCSSaveGame::FindStageRecord(int32 ChapterNumber, int32 StageNumber)
{
    for (FStageRecord& Record : StageRecords)
    {
        if (Record.ChapterNumber == ChapterNumber && Record.StageNumber == StageNumber)
        {
            return &Record;
        }
    }
    return nullptr;
}

const FStageRecord* UCSSaveGame::FindStageRecord(int32 ChapterNumber, int32 StageNumber) const
{
    for (const FStageRecord& Record : StageRecords)
    {
        if (Record.ChapterNumber == ChapterNumber && Record.StageNumber == StageNumber)
        {
            return &Record;
        }
    }
    return nullptr;
}

void UCSSaveGame::EnsureStageRecordExists(int32 ChapterNumber, int32 StageNumber)
{
    if (!FindStageRecord(ChapterNumber, StageNumber))
    {
        StageRecords.Add(FStageRecord(ChapterNumber, StageNumber));
    }
}

void UCSSaveGame::InitializeDefaultStages()
{
    // Initialize all stages (4 chapters * 5 stages = 20 stages)
    for (int32 Chapter = 1; Chapter <= 4; Chapter++)
    {
        for (int32 Stage = 1; Stage <= 5; Stage++)
        {
            StageRecords.Add(FStageRecord(Chapter, Stage));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Initialized save game with %d stage records"), StageRecords.Num());
}