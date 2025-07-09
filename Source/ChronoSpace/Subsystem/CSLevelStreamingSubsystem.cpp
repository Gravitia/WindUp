// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Game/CSGameState.h"
#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/DataTable.h"

UCSLevelStreamingSubsystem::UCSLevelStreamingSubsystem()
{
    CurrentLevelIndex = 0;
    CurrentStreamingLevel = nullptr;
    CurrentChapter = 1;
    CurrentStage = 1;
    StageDataTable = nullptr;
    CurrentSpawnPosition = FVector::ZeroVector;
}

void UCSLevelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem initialized"));
}

void UCSLevelStreamingSubsystem::Deinitialize()
{
    if (CurrentStreamingLevel)
    {
        UnloadCurrentLevel();
    }

    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem deinitialized"));
}

bool UCSLevelStreamingSubsystem::StreamLevel(int32 ChapterNumber, int32 StageNumber)
{
    // Only host/server can stream levels in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot stream levels"));
        return false;
    }

    // 현재 레벨이 있다면 언로드
    if (CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Log, TEXT("Unloading current level before streaming new one"));
        UnloadCurrentLevel();
    }

    // 데이터 테이블에서 스테이지 정보 가져오기
    FStageData* StageData = GetStageDataFromTable(ChapterNumber, StageNumber);
    if (!StageData)
    {
        UE_LOG(LogTemp, Error, TEXT("No stage data found for C%d_S%d in DataTable"), ChapterNumber, StageNumber);
        return false;
    }

    // 레벨 경로 결정
    FString ActualLevelPath = GetActualLevelPath(StageData);
    if (ActualLevelPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot determine level path for C%d_S%d"), ChapterNumber, StageNumber);
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid world for level streaming"));
        return false;
    }

    // 레벨 스트리밍 시도 로그
    UE_LOG(LogTemp, Log, TEXT("Attempting to stream level: %s for C%d_S%d"),
        *ActualLevelPath, ChapterNumber, StageNumber);

    // UE5 레벨 스트리밍
    bool bLoadSuccess = false;
    CurrentStreamingLevel = ULevelStreamingDynamic::LoadLevelInstance(
        World,
        ActualLevelPath,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        bLoadSuccess
    );

    if (CurrentStreamingLevel && bLoadSuccess)
    {
        // 스트리밍 성공
        CurrentChapter = ChapterNumber;
        CurrentStage = StageNumber;
        CurrentSpawnPosition = StageData->SpawnPosition;

        // Progress subsystem 업데이트
        UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
        if (ProgressSubsystem)
        {
            ProgressSubsystem->SetLastPlayedStage(ChapterNumber, StageNumber);
        }

        // 이벤트 브로드캐스트
        OnLevelStreamed.Broadcast(ChapterNumber, StageNumber);

        UE_LOG(LogTemp, Log, TEXT("Level streamed successfully: %s for C%d_S%d at position %s"),
            *ActualLevelPath, ChapterNumber, StageNumber, *StageData->SpawnPosition.ToString());

        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to stream level: %s for C%d_S%d (LoadSuccess: %s)"),
            *ActualLevelPath, ChapterNumber, StageNumber, bLoadSuccess ? TEXT("true") : TEXT("false"));
        return false;
    }
}

bool UCSLevelStreamingSubsystem::UnloadCurrentLevel()
{
    // Only host/server can unload levels in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot unload levels"));
        return false;
    }

    if (!CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("No current level to unload"));
        return false;
    }

    int32 UnloadedChapter = CurrentChapter;
    int32 UnloadedStage = CurrentStage;

    CurrentStreamingLevel->SetShouldBeLoaded(false);
    CurrentStreamingLevel->SetShouldBeVisible(false);
    CurrentStreamingLevel = nullptr;

    OnLevelUnloaded.Broadcast(UnloadedChapter, UnloadedStage);
    UE_LOG(LogTemp, Log, TEXT("Level unloaded successfully: C%d_S%d"), UnloadedChapter, UnloadedStage);

    return true;
}

FVector UCSLevelStreamingSubsystem::GetSpawnPosition() const
{
    return CurrentSpawnPosition;
}

void UCSLevelStreamingSubsystem::SetStageDataTable(UDataTable* InStageDataTable)
{
    StageDataTable = InStageDataTable;

    if (StageDataTable)
    {
        UE_LOG(LogTemp, Log, TEXT("Stage data table set successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Stage data table set to null"));
    }
}

bool UCSLevelStreamingSubsystem::StreamLevelByName(const FString& LevelName, const FVector& SpawnPosition)
{
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot stream levels"));
        return false;
    }

    if (LevelName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Level name is empty"));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid world for level streaming"));
        return false;
    }

    // 현재 레벨 언로드
    if (CurrentStreamingLevel)
    {
        UnloadCurrentLevel();
    }

    UE_LOG(LogTemp, Log, TEXT("Streaming level by name: %s"), *LevelName);

    bool bLoadSuccess = false;
    CurrentStreamingLevel = ULevelStreamingDynamic::LoadLevelInstance(
        World,
        LevelName,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        bLoadSuccess
    );

    if (CurrentStreamingLevel && bLoadSuccess)
    {
        CurrentSpawnPosition = SpawnPosition;
        UE_LOG(LogTemp, Log, TEXT("Level streamed successfully by name: %s"), *LevelName);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to stream level by name: %s (LoadSuccess: %s)"),
            *LevelName, bLoadSuccess ? TEXT("true") : TEXT("false"));
        return false;
    }
}

// === C++ Only Functions ===

FStageData* UCSLevelStreamingSubsystem::GetStageDataFromTable(int32 ChapterNumber, int32 StageNumber) const
{
    if (!StageDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("StageDataTable is null! Please assign DataTable in Blueprint or C++"));
        return nullptr;
    }

    // 데이터 테이블에서 모든 행 가져오기
    TArray<FStageData*> AllRows;
    StageDataTable->GetAllRows<FStageData>(TEXT("GetStageDataFromTable"), AllRows);

    UE_LOG(LogTemp, Log, TEXT("Searching for C%d_S%d in DataTable with %d rows"),
        ChapterNumber, StageNumber, AllRows.Num());

    // 해당 챕터/스테이지 찾기
    for (FStageData* Row : AllRows)
    {
        if (Row && Row->ChapterNumber == ChapterNumber && Row->StageNumber == StageNumber)
        {
            UE_LOG(LogTemp, Log, TEXT("Found stage data: C%d_S%d -> Name: %s, Path: %s, Spawn: %s"),
                Row->ChapterNumber, Row->StageNumber, *Row->LevelName, *Row->LevelPath, *Row->SpawnPosition.ToString());
            return Row;
        }
    }

    // 찾지 못한 경우 디버그 정보 출력
    UE_LOG(LogTemp, Warning, TEXT("Stage C%d_S%d not found. Available stages:"), ChapterNumber, StageNumber);
    for (FStageData* Row : AllRows)
    {
        if (Row)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - C%d_S%d: Name=%s, Path=%s"),
                Row->ChapterNumber, Row->StageNumber, *Row->LevelName, *Row->LevelPath);
        }
    }

    return nullptr;
}

FString UCSLevelStreamingSubsystem::GetActualLevelPath(const FStageData* StageData) const
{
    if (!StageData)
    {
        UE_LOG(LogTemp, Error, TEXT("StageData is null"));
        return FString();
    }

    // LevelPath가 우선순위
    if (!StageData->LevelPath.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Using LevelPath: %s"), *StageData->LevelPath);
        return StageData->LevelPath;
    }

    // LevelPath가 비어있으면 LevelName 사용
    if (!StageData->LevelName.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Using LevelName: %s"), *StageData->LevelName);
        return StageData->LevelName;
    }

    // 둘 다 비어있으면 빈 문자열 반환
    UE_LOG(LogTemp, Error, TEXT("Both LevelPath and LevelName are empty"));
    return FString();
}

void UCSLevelStreamingSubsystem::CompleteCurrentStage()
{
    // Only host/server can complete stages in multiplayer
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot complete stages"));
        return;
    }

    UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
    if (ProgressSubsystem)
    {
        ProgressSubsystem->ClearStage(CurrentChapter, CurrentStage);
        UE_LOG(LogTemp, Log, TEXT("Stage completed: C%d_S%d"), CurrentChapter, CurrentStage);
    }
}

void UCSLevelStreamingSubsystem::GetCurrentStage(int32& OutChapter, int32& OutStage) const
{
    OutChapter = CurrentChapter;
    OutStage = CurrentStage;
}

bool UCSLevelStreamingSubsystem::IsLevelStreaming() const
{
    return CurrentStreamingLevel != nullptr;
}

bool UCSLevelStreamingSubsystem::IsMultiplayerClient() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetNetMode() == NM_Client;
    }
    return false;
}

UCSGameProgressSubsystem* UCSLevelStreamingSubsystem::GetProgressSubsystem() const
{
    return GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
}

ACSGameState* UCSLevelStreamingSubsystem::GetCSGameState() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return Cast<ACSGameState>(World->GetGameState());
    }
    return nullptr;
}