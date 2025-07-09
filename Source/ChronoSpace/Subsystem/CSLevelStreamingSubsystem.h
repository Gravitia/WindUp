// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/DataTable.h"
#include "CSLevelStreamingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelStreamed, int32, ChapterNumber, int32, StageNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelUnloaded, int32, ChapterNumber, int32, StageNumber);

// 스테이지 데이터 구조체
USTRUCT(BlueprintType)
struct CHRONOSPACE_API FStageData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    int32 ChapterNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    int32 StageNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    FString LevelName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    FString LevelPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    FVector SpawnPosition;

    FStageData()
    {
        ChapterNumber = 1;
        StageNumber = 1;
        LevelName = TEXT("");
        LevelPath = TEXT("");
        SpawnPosition = FVector::ZeroVector;
    }
};


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

    UPROPERTY(BlueprintReadOnly, Category = "Level Streaming")
    int32 CurrentLevelIndex;

    UPROPERTY()
    ULevelStreamingDynamic* CurrentStreamingLevel;

    // 데이터 테이블 참조
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Streaming")
    UDataTable* StageDataTable;

    // 현재 스폰 위치
    UPROPERTY(BlueprintReadOnly, Category = "Level Streaming")
    FVector CurrentSpawnPosition;

    // 현재 챕터와 스테이지
    int32 CurrentChapter;
    int32 CurrentStage;

public:
    // === Core Level Streaming Functions ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool StreamLevel(int32 ChapterNumber, int32 StageNumber);

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool UnloadCurrentLevel();

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    FVector GetSpawnPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void SetStageDataTable(UDataTable* InStageDataTable);

    // 레벨 이름으로 직접 스트리밍 (헬퍼 함수)
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool StreamLevelByName(const FString& LevelName, const FVector& SpawnPosition = FVector::ZeroVector);

    // === Data Table Functions (C++ Only) ===
    // C++에서만 사용하는 함수 (UFUNCTION 제거)
    FStageData* GetStageDataFromTable(int32 ChapterNumber, int32 StageNumber) const;

    // C++에서만 사용하는 함수 (UFUNCTION 제거)
    FString GetActualLevelPath(const FStageData* StageData) const;

    // === Stage Integration ===
    UFUNCTION(BlueprintCallable, Category = "Stage Integration")
    void CompleteCurrentStage();

    // === Getters ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void GetCurrentStage(int32& OutChapter, int32& OutStage) const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool IsLevelStreaming() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    UDataTable* GetStageDataTable() const { return StageDataTable; }

    // === Events ===
    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelStreamed OnLevelStreamed;

    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelUnloaded OnLevelUnloaded;

private:
    bool IsMultiplayerClient() const;
    class UCSGameProgressSubsystem* GetProgressSubsystem() const;
    class ACSGameState* GetCSGameState() const;
};
