// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CSGameProgressSubsystem.generated.h"

class UCSSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameLoaded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameSaved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewGameStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageCleared, int32, Chapter, int32, Stage);

/**
 * Facade for save/load and stage progression.
 * Auto-saves to disk after every progress mutation.
 * Host-only in multiplayer; clients ignore mutation calls.
 */
UCLASS()
class CHRONOSPACE_API UCSGameProgressSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === Save / Load ===
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveGame();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadGame();

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool NewGame();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Save")
    bool DoesSaveExist() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Save")
    UCSSaveGame* GetSaveGame() const { return CurrentSaveGame; }

    // === Stage Progress ===
    UFUNCTION(BlueprintCallable, Category = "Progress")
    void MarkStageCleared(int32 Chapter, int32 Stage);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress")
    bool IsStageCleared(int32 Chapter, int32 Stage) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress")
    bool IsStageUnlocked(int32 Chapter, int32 Stage) const;

    // Stage that follows (Chapter, Stage) per the stage data table. False if there is none.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress")
    bool GetNextStage(int32 Chapter, int32 Stage, int32& OutChapter, int32& OutStage) const;

    // === Last Played ===
    UFUNCTION(BlueprintCallable, Category = "Progress")
    void SetLastPlayedStage(int32 Chapter, int32 Stage);

    UFUNCTION(BlueprintCallable, Category = "Progress")
    void GetLastPlayedStage(int32& OutChapter, int32& OutStage) const;

    // === Events ===
    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnGameLoaded OnGameLoaded;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnGameSaved OnGameSaved;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnNewGameStarted OnNewGameStarted;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnStageCleared OnStageCleared;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Save")
    FString SaveSlotName = TEXT("ChronoSpaceSave");

    UPROPERTY(EditDefaultsOnly, Category = "Save")
    int32 UserIndex = 0;

    UPROPERTY()
    TObjectPtr<UCSSaveGame> CurrentSaveGame;

private:
    bool IsClient() const;
    void CreateNewSaveGameObject();
};
