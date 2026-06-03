// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CSSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FStageRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
    int32 Chapter = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
    int32 Stage = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
    bool bCleared = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
    int32 DeathCount = 0;

    FStageRecord() = default;
    FStageRecord(int32 InChapter, int32 InStage)
        : Chapter(InChapter), Stage(InStage) {}
};

/**
 * Pure data container for persisted game progress.
 * All progression logic lives in UCSGameProgressSubsystem.
 */
UCLASS()
class CHRONOSPACE_API UCSSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 LatestSaveVersion = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save Info")
    int32 SaveVersion = LatestSaveVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
    TArray<FStageRecord> StageRecords;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
    int32 LastPlayedChapter = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
    int32 LastPlayedStage = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 TotalDeaths = 0;

    FStageRecord* FindRecord(int32 Chapter, int32 Stage);
    const FStageRecord* FindRecord(int32 Chapter, int32 Stage) const;
    FStageRecord& FindOrAddRecord(int32 Chapter, int32 Stage);
};
