// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/LevelStreamingDynamic.h"
#include "CSLevelStreamingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelStreamed, int32, ChapterNumber, int32, StageNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelUnloaded, int32, ChapterNumber, int32, StageNumber);

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSLevelStreamingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    UCSLevelStreamingSubsystem();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Streaming")
    TArray<TSoftObjectPtr<UWorld>> LevelAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Streaming")
    TArray<FVector> SpawnPositions;

    UPROPERTY(BlueprintReadOnly, Category = "Level Streaming")
    int32 CurrentLevelIndex;

    UPROPERTY()
    ULevelStreamingDynamic* CurrentStreamingLevel;

public:
    // === Core Level Streaming Functions ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool StreamLevel(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool UnloadCurrentLevel();

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    FVector GetSpawnPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void SetLevelData(const TArray<TSoftObjectPtr<UWorld>>& InLevelAssets, const TArray<FVector>& InSpawnPositions);

    // === Stage Integration ===
    UFUNCTION(BlueprintCallable, Category = "Stage Integration")
    void CompleteCurrentStage();

    // === Getters ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void GetCurrentStage(int32& OutChapter, int32& OutStage) const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool IsLevelStreaming() const;

    // === Events ===
    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelStreamed OnLevelStreamed;

    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelUnloaded OnLevelUnloaded;

private:
    int32 CalculateLevelIndex(int32 ChapterNumber, int32 StageNumber) const;
    bool IsMultiplayerClient() const;
    class UCSGameProgressSubsystem* GetProgressSubsystem() const;
    class ACSGameState* GetCSGameState() const;

    int32 CurrentChapter;
    int32 CurrentStage;
};
