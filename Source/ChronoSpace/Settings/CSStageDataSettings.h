// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/World.h"
#include "CSStageDataSettings.generated.h"

/** One playable stage: its number within a chapter, the level to travel to, and a UI label. */
USTRUCT(BlueprintType)
struct FCSStageDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Config, Category = "Stage", meta = (ClampMin = "1"))
    int32 Stage = 1;

    // The level to ServerTravel into for this stage.
    UPROPERTY(EditAnywhere, Config, Category = "Stage")
    TSoftObjectPtr<UWorld> Level;

    // Optional label shown in the stage-select UI. Falls back to "C<n>-S<n>" if empty.
    UPROPERTY(EditAnywhere, Config, Category = "Stage")
    FText DisplayName;
};

/** One chapter: its number and the ordered stages it contains. */
USTRUCT(BlueprintType)
struct FCSChapterDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Config, Category = "Chapter", meta = (ClampMin = "1"))
    int32 Chapter = 1;

    UPROPERTY(EditAnywhere, Config, Category = "Chapter")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Config, Category = "Chapter")
    TArray<FCSStageDef> Stages;
};

/** A destination the stage-select UI can present. Built per-client, not replicated. */
USTRUCT(BlueprintType)
struct FCSStageOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stage")
    int32 Chapter = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stage")
    int32 Stage = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stage")
    FText DisplayName;

    // True if the player may travel here right now.
    UPROPERTY(BlueprintReadOnly, Category = "Stage")
    bool bSelectable = false;

    // True for the normal "next stage" (so the UI can highlight it amid debug entries).
    UPROPERTY(BlueprintReadOnly, Category = "Stage")
    bool bIsNextStage = false;
};

/**
 * Central, project-wide table of chapters -> stages -> level mapping.
 *
 * Single source of truth for: which level a (chapter, stage) maps to, what the
 * last stage of a chapter is, what the next/previous stage is, and therefore how
 * chapter unlocking works. Edited under Project Settings > Game > ChronoSpace Stage Data.
 * Read from code via UCSStageDataSettings::Get() — no asset reference wiring needed.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ChronoSpace Stage Data"))
class CHRONOSPACE_API UCSStageDataSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // 4 chapters of ~10 stages each. Order within a chapter is by Stage number.
    UPROPERTY(EditAnywhere, Config, Category = "Stages")
    TArray<FCSChapterDef> Chapters;

    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    /** Convenience accessor for the CDO (the live, edited settings object). */
    static const UCSStageDataSettings* Get();

    const FCSChapterDef* FindChapter(int32 Chapter) const;
    const FCSStageDef* FindStageDef(int32 Chapter, int32 Stage) const;
    bool IsValidStage(int32 Chapter, int32 Stage) const;

    /** Highest stage number defined in a chapter, or 0 if the chapter is missing/empty. */
    int32 GetLastStageNumber(int32 Chapter) const;
    bool IsLastStageOfChapter(int32 Chapter, int32 Stage) const;

    /** Overall first stage of the game (smallest chapter, smallest stage). */
    bool GetFirstStage(int32& OutChapter, int32& OutStage) const;

    /** Stage that follows (Chapter, Stage): next in the chapter, else first of the next chapter. */
    bool GetNextStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const;

    /** Stage that leads into (Chapter, Stage): previous in the chapter, else last of the prior chapter. */
    bool GetPrevStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const;

    /** Long package name for ServerTravel, or empty if unmapped. */
    FString GetStageTravelURL(int32 Chapter, int32 Stage) const;

    /** UI label, falling back to "C<chapter>-S<stage>". */
    FText GetStageDisplayName(int32 Chapter, int32 Stage) const;
};
