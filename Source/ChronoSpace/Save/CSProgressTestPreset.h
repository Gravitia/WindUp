// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSProgressTestPreset.generated.h"

/**
 * Editor-only test helper for the save/load system.
 *
 * This is NOT the shipped save storage — the game still saves to a local
 * .sav slot through UCSGameProgressSubsystem. This asset just lets you
 * author a progress state ("cleared up to chapter X, stage Y") in the
 * editor and stamp it into the real save slot with one button, so you can
 * test mid-game progression in PIE without playing through everything.
 *
 * Usage:
 *   1. Fill StagesPerChapter, ClearedUpTo*, and LastPlayed*.
 *   2. Click "Apply To Save Slot" in the details panel.
 *   3. Press Play — the subsystem loads the slot as if you'd earned it.
 *   "Delete Save Slot" wipes it back to a fresh game.
 */
UCLASS(BlueprintType)
class CHRONOSPACE_API UCSProgressTestPreset : public UDataAsset
{
    GENERATED_BODY()

public:
    // Must match UCSGameProgressSubsystem's slot settings so the game loads what we write.
    UPROPERTY(EditAnywhere, Category = "Target Slot")
    FString SaveSlotName = TEXT("ChronoSpaceSave");

    UPROPERTY(EditAnywhere, Category = "Target Slot")
    int32 UserIndex = 0;

    // Chapter number -> number of stages in that chapter (e.g. {1: 5, 2: 4}).
    // Used to fully clear every chapter before ClearedUpToChapter.
    UPROPERTY(EditAnywhere, Category = "Test Progress")
    TMap<int32, int32> StagesPerChapter;

    // Everything up to and including (ClearedUpToChapter, ClearedUpToStage) is marked cleared.
    // Set ClearedUpToStage to 0 to clear no stages in the final chapter (fresh start of it).
    UPROPERTY(EditAnywhere, Category = "Test Progress", meta = (ClampMin = "1"))
    int32 ClearedUpToChapter = 1;

    UPROPERTY(EditAnywhere, Category = "Test Progress", meta = (ClampMin = "0"))
    int32 ClearedUpToStage = 0;

    // Where "Continue" should drop the player. Leave both at 0 to auto-pick the
    // stage right after the last cleared one.
    UPROPERTY(EditAnywhere, Category = "Test Progress", meta = (ClampMin = "0"))
    int32 LastPlayedChapter = 0;

    UPROPERTY(EditAnywhere, Category = "Test Progress", meta = (ClampMin = "0"))
    int32 LastPlayedStage = 0;

#if WITH_EDITOR
    // Builds a UCSSaveGame from the fields above and writes it to the save slot.
    UFUNCTION(CallInEditor, Category = "Save Test")
    void ApplyToSaveSlot();

    // Deletes the save slot so the next play starts a brand-new game.
    UFUNCTION(CallInEditor, Category = "Save Test")
    void DeleteSaveSlot();
#endif
};
