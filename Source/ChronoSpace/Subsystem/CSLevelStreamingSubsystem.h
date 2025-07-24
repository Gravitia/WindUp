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
    FVector WorldSpawnPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
    FVector CharacterSpawnPosition;

    FStageData()
    {
        ChapterNumber = 1;
        StageNumber = 1;
        LevelName = TEXT("");
        LevelPath = TEXT("");
        WorldSpawnPosition = FVector::ZeroVector;
        CharacterSpawnPosition = FVector::ZeroVector;
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

    // 데이터 테이블 참조
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Streaming")
    UDataTable* StageDataTable;

    // 현재 스폰 위치들 (정보 제공용)
    UPROPERTY(BlueprintReadOnly, Category = "Level Streaming")
    FVector CurrentSpawnPosition;

    UPROPERTY(BlueprintReadOnly, Category = "Level Streaming")
    FVector CurrentCharacterSpawnPosition;

    // 현재 챕터와 스테이지
    int32 CurrentChapter;
    int32 CurrentStage;

public:
    // === 데이터 제공 함수들 ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    FVector GetSpawnPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    FVector GetCharacterSpawnPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void SetStageDataTable(UDataTable* InStageDataTable);

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    UDataTable* GetStageDataTable() const { return StageDataTable; }

    // === Data Table Functions ===
    FStageData* GetStageDataFromTable(int32 ChapterNumber, int32 StageNumber) const;
    FString GetActualLevelPath(const FStageData* StageData) const;

    // === Stage Integration ===
    UFUNCTION(BlueprintCallable, Category = "Stage Integration")
    void CompleteCurrentStage();

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    void GetCurrentStage(int32& OutChapter, int32& OutStage) const;

    // === 상태 업데이트 (Trigger에서 호출됨) ===
    void UpdateCurrentStage(int32 ChapterNumber, int32 StageNumber,
        FVector WorldSpawn, FVector CharacterSpawn);

    // === 레거시 호환성 상태 확인 함수들 ===
    UFUNCTION(BlueprintCallable, Category = "Level Streaming", meta = (DeprecatedFunction))
    bool IsLevelStreaming() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming", meta = (DeprecatedFunction))
    bool IsAsyncStreamingInProgress() const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool IsCurrentStage(int32 ChapterNumber, int32 StageNumber) const;

    UFUNCTION(BlueprintCallable, Category = "Level Streaming")
    bool IsLevelActuallyLoaded() const;

    // === Events (UI 업데이트용) ===
    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelStreamed OnLevelStreamed;

    UPROPERTY(BlueprintAssignable, Category = "Level Events")
    FOnLevelUnloaded OnLevelUnloaded;

private:
    class UCSGameProgressSubsystem* GetProgressSubsystem() const;
};