// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/DataTable.h"

UCSLevelStreamingSubsystem::UCSLevelStreamingSubsystem()
{
    // /Script/Engine.DataTable'/Game/20_Data/DTCS_StageTable.DTCS_StageTable'
    static ConstructorHelpers::FObjectFinder<UDataTable> StageDataObj(
        TEXT("DataTable'/Game/20_Data/DTCS_StageTable.DTCS_StageTable'")
    );

    if (StageDataObj.Succeeded())
    {
        StageDataTable = StageDataObj.Object;
        UE_LOG(LogTemp, Log, TEXT("StageDataObj.Succeeded"));
    }

    CurrentLevelIndex = 0;
    CurrentStreamingLevel = nullptr;
    CurrentChapter = 1;
    CurrentStage = 1;
    CurrentSpawnPosition = FVector::ZeroVector;
    CurrentCharacterSpawnPosition = FVector::ZeroVector;
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

// === Core Level Streaming Functions ===

bool UCSLevelStreamingSubsystem::StreamLevel(int32 ChapterNumber, int32 StageNumber)
{
    //  클라이언트 제한 제거 - 모든 머신에서 레벨 스트리밍 허용
    /*
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot stream levels directly - use RequestStreamLevel instead"));
        return false;
    }
    */

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
        StageData->WorldSpawnPosition,
        FRotator::ZeroRotator,
        bLoadSuccess
    );

    if (CurrentStreamingLevel && bLoadSuccess)
    {
        // 스트리밍 성공
        CurrentChapter = ChapterNumber;
        CurrentStage = StageNumber;
        CurrentSpawnPosition = StageData->WorldSpawnPosition;         // 레벨 스트리밍용 위치
        CurrentCharacterSpawnPosition = StageData->CharacterSpawnPosition; // 캐릭터 이동용 위치

        // Progress subsystem 업데이트
        UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
        if (ProgressSubsystem)
        {
            ProgressSubsystem->SetLastPlayedStage(ChapterNumber, StageNumber);
        }

        // 이벤트 브로드캐스트
        OnLevelStreamed.Broadcast(ChapterNumber, StageNumber);

        UE_LOG(LogTemp, Log, TEXT("Level streamed successfully: %s for C%d_S%d at WorldSpawn:%s, CharacterSpawn:%s"),
            *ActualLevelPath, ChapterNumber, StageNumber,
            *StageData->WorldSpawnPosition.ToString(), *StageData->CharacterSpawnPosition.ToString());

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
    //  클라이언트 제한 제거 - 모든 머신에서 언로드 허용
    /*
    if (IsMultiplayerClient())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clients cannot unload levels directly - use RequestUnloadCurrentLevel instead"));
        return false;
    }
    */

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

    // 이벤트 브로드캐스트
    OnLevelUnloaded.Broadcast(UnloadedChapter, UnloadedStage);

    UE_LOG(LogTemp, Log, TEXT("Level unloaded successfully: C%d_S%d"), UnloadedChapter, UnloadedStage);

    return true;
}

// === Simple Request Functions ===

void UCSLevelStreamingSubsystem::RequestStreamLevel(int32 ChapterNumber, int32 StageNumber)
{
    // 복잡한 분기 제거 - 그냥 바로 실행
    UE_LOG(LogTemp, Log, TEXT("Requesting to stream level C%d_S%d"), ChapterNumber, StageNumber);
    StreamLevel(ChapterNumber, StageNumber);
}

void UCSLevelStreamingSubsystem::RequestUnloadCurrentLevel()
{
    //  복잡한 분기 제거 - 그냥 바로 실행
    UE_LOG(LogTemp, Log, TEXT("Requesting to unload current level"));
    UnloadCurrentLevel();
}

// === Other Functions ===

FVector UCSLevelStreamingSubsystem::GetSpawnPosition() const
{
    return CurrentSpawnPosition;
}

FVector UCSLevelStreamingSubsystem::GetCharacterSpawnPosition() const
{
    return CurrentCharacterSpawnPosition;
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

// === Data Table Functions ===

FStageData* UCSLevelStreamingSubsystem::GetStageDataFromTable(int32 ChapterNumber, int32 StageNumber) const
{
    if (!StageDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("StageDataTable is null! Please assign DataTable in Blueprint or C++"));
        return nullptr;
    }

    TArray<FStageData*> AllRows;
    StageDataTable->GetAllRows<FStageData>(TEXT("GetStageDataFromTable"), AllRows);

    UE_LOG(LogTemp, Log, TEXT("Searching for C%d_S%d in DataTable with %d rows"),
        ChapterNumber, StageNumber, AllRows.Num());

    for (FStageData* Row : AllRows)
    {
        if (Row && Row->ChapterNumber == ChapterNumber && Row->StageNumber == StageNumber)
        {
            UE_LOG(LogTemp, Log, TEXT("Found stage data: C%d_S%d -> Name: %s, Path: %s, Spawn: %s"),
                Row->ChapterNumber, Row->StageNumber, *Row->LevelName, *Row->LevelPath, *Row->WorldSpawnPosition.ToString());
            return Row;
        }
    }

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

    if (!StageData->LevelPath.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Using LevelPath: %s"), *StageData->LevelPath);
        return StageData->LevelPath;
    }

    if (!StageData->LevelName.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("Using LevelName: %s"), *StageData->LevelName);
        return StageData->LevelName;
    }

    UE_LOG(LogTemp, Error, TEXT("Both LevelPath and LevelName are empty"));
    return FString();
}

void UCSLevelStreamingSubsystem::CompleteCurrentStage()
{
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

UCSGameProgressSubsystem* UCSLevelStreamingSubsystem::GetProgressSubsystem() const
{
    return GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
}