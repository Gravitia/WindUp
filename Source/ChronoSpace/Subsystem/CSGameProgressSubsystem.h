// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CSGameProgressSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGameProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	

public:
    UCSGameProgressSubsystem();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Current save game object
    UPROPERTY(BlueprintReadOnly, Category = "Save Game")
    class UCSSaveGame* CurrentSaveGame;

    // Default save slot name
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save Settings")
    FString DefaultSaveSlotName = TEXT("ChronoSpaceSave");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save Settings")
    int32 DefaultUserIndex = 0;

public:
    // === Core Save/Load Functions ===
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveGame();

    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadGame();

    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool NewGame();

    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool DoesSaveExist() const;

    // === Stage Management ===
    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    void ClearStage(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    bool IsStageCleared(int32 ChapterNumber, int32 StageNumber) const;

    UFUNCTION(BlueprintCallable, Category = "Stage Management")
    bool IsStageUnlocked(int32 ChapterNumber, int32 StageNumber) const;

    // === Continue System ===
    UFUNCTION(BlueprintCallable, Category = "Continue")
    void SetLastPlayedStage(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Continue")
    void GetLastPlayedStage(int32& OutChapter, int32& OutStage) const;

    // === Statistics ===
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void IncrementDeathCount();

    // === UI Integration ===
    UFUNCTION(BlueprintCallable, Category = "UI")
    bool LoadGameFromUI();

    UFUNCTION(BlueprintCallable, Category = "UI")
    bool NewGameFromUI();

    UFUNCTION(BlueprintCallable, Category = "UI")
    UCSSaveGame* GetSaveGameObject() const { return CurrentSaveGame; }

    // === Events ===
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameLoaded);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameSaved);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewGameStarted);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageCleared, int32, ChapterNumber, int32, StageNumber);

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnGameLoaded OnGameLoaded;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnGameSaved OnGameSaved;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnNewGameStarted OnNewGameStarted;

    UPROPERTY(BlueprintAssignable, Category = "Progress Events")
    FOnStageCleared OnStageCleared;

private:
    void CreateNewSaveGame();
    bool IsMultiplayerClient() const;
    class ACSGameState* GetCSGameState() const;
};
