// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"

UCSLevelStreamingSubsystem::UCSLevelStreamingSubsystem()
{
    // 데이터 테이블 로드
    static ConstructorHelpers::FObjectFinder<UDataTable> StageDataObj(
        TEXT("DataTable'/Game/20_Data/DTCS_StageTable.DTCS_StageTable'")
    );

    if (StageDataObj.Succeeded())
    {
        StageDataTable = StageDataObj.Object;
        UE_LOG(LogTemp, Log, TEXT("StageDataObj.Succeeded"));
    }

    // 기본 값 초기화
    CurrentChapter = 1;
    CurrentStage = 1;
    CurrentSpawnPosition = FVector::ZeroVector;
    CurrentCharacterSpawnPosition = FVector::ZeroVector;
}

void UCSLevelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem initialized (Data Provider Mode)"));
}

void UCSLevelStreamingSubsystem::Deinitialize()
{
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem deinitialized"));
}



// === 데이터 제공 함수들 (계속 사용됨) ===

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

// === Data Table Functions (계속 사용됨) ===

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

// === Progress Integration ===

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

// === 상태 업데이트 함수 (Trigger에서 호출됨) ===

void UCSLevelStreamingSubsystem::UpdateCurrentStage(int32 ChapterNumber, int32 StageNumber,
    FVector WorldSpawn, FVector CharacterSpawn)
{
    CurrentChapter = ChapterNumber;
    CurrentStage = StageNumber;
    CurrentSpawnPosition = WorldSpawn;
    CurrentCharacterSpawnPosition = CharacterSpawn;

    UE_LOG(LogTemp, Log, TEXT("Subsystem stage updated: C%d_S%d"), ChapterNumber, StageNumber);
}

// === 레거시 호환성 함수들 ===

bool UCSLevelStreamingSubsystem::IsLevelStreaming() const
{
    // 이제 Trigger에서 관리하므로 항상 false 반환
    return false;
}

bool UCSLevelStreamingSubsystem::IsAsyncStreamingInProgress() const
{
    // 이제 Trigger에서 관리하므로 항상 false 반환
    return false;
}

bool UCSLevelStreamingSubsystem::IsCurrentStage(int32 ChapterNumber, int32 StageNumber) const
{
    return CurrentChapter == ChapterNumber && CurrentStage == StageNumber;
}

bool UCSLevelStreamingSubsystem::IsLevelActuallyLoaded() const
{
    // 이제 Trigger에서 관리하므로 기본적으로 true 반환
    return true;
}

UCSGameProgressSubsystem* UCSLevelStreamingSubsystem::GetProgressSubsystem() const
{
    return GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
}