// Fill out your copyright notice in the Description page of Project Settings.


#include "Save/CSSaveGame.h"

FStageRecord* UCSSaveGame::FindRecord(int32 Chapter, int32 Stage)
{
    return StageRecords.FindByPredicate([Chapter, Stage](const FStageRecord& R)
    {
        return R.Chapter == Chapter && R.Stage == Stage;
    });
}

const FStageRecord* UCSSaveGame::FindRecord(int32 Chapter, int32 Stage) const
{
    return StageRecords.FindByPredicate([Chapter, Stage](const FStageRecord& R)
    {
        return R.Chapter == Chapter && R.Stage == Stage;
    });
}

FStageRecord& UCSSaveGame::FindOrAddRecord(int32 Chapter, int32 Stage)
{
    if (FStageRecord* Existing = FindRecord(Chapter, Stage))
    {
        return *Existing;
    }
    return StageRecords.Emplace_GetRef(Chapter, Stage);
}
