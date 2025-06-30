// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CSSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FStageRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Info")
    int32 ChapterNumber;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Info")
    int32 StageNumber;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clear Status")
    bool bStageCleared;

    FStageRecord()
    {
        ChapterNumber = 1;
        StageNumber = 1;
        bStageCleared = false;
    }

    FStageRecord(int32 InChapter, int32 InStage)
    {
        ChapterNumber = InChapter;
        StageNumber = InStage;
        bStageCleared = false;
    }

    // Generate stage ID for easy lookup
    FString GetStageID() const
    {
        return FString::Printf(TEXT("C%d_S%d"), ChapterNumber, StageNumber);
    }
};


/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
    UCSSaveGame();

    // === Save File Info ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save Info")
    FString SaveSlotName;

    // === Game Progress ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
    TArray<FStageRecord> StageRecords;

    // === Continue ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Continue")
    int32 LastPlayedChapter;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Continue")
    int32 LastPlayedStage;

    // === Statistics ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Statistics")
    int32 TotalDeaths;

public:
    // === Stage Management ===
    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    bool IsStageCleared(int32 ChapterNumber, int32 StageNumber) const;

    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    void ClearStage(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    bool IsStageUnlocked(int32 ChapterNumber, int32 StageNumber) const;

    // === Continue Info ===
    UFUNCTION(BlueprintCallable, Category = "Continue")
    void SetLastPlayedStage(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Continue")
    void GetLastPlayedStage(int32& OutChapter, int32& OutStage) const;

    // === Statistics ===
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void IncrementDeathCount();

private:
    // Internal helper functions
    FStageRecord* FindStageRecord(int32 ChapterNumber, int32 StageNumber);
    const FStageRecord* FindStageRecord(int32 ChapterNumber, int32 StageNumber) const;
    void EnsureStageRecordExists(int32 ChapterNumber, int32 StageNumber);
    void InitializeDefaultStages();
};

