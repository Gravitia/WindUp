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
    // 비동기 상태 정리
    if (bIsAsyncStreaming)
    {
        bIsAsyncStreaming = false;
        PendingChapterNumber = -1;
        PendingStageNumber = -1;
        PendingStageData = nullptr;
    }

    // 현재 레벨 정리
    if (CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Log, TEXT("Cleaning up current streaming level on deinitialize"));

        // 델리게이트 정리
        CurrentStreamingLevel->OnLevelLoaded.RemoveDynamic(this, &UCSLevelStreamingSubsystem::OnAsyncLevelLoaded);

        // 기존 UnloadCurrentLevel 호출
        UnloadCurrentLevel();
    }

    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingSubsystem deinitialized"));
}

// === Core Level Streaming Functions ===

bool UCSLevelStreamingSubsystem::StreamLevel(int32 ChapterNumber, int32 StageNumber)
{
    // 이미 비동기 스트리밍 중이면 대기
    if (bIsAsyncStreaming)
    {
        UE_LOG(LogTemp, Warning, TEXT("Async level streaming already in progress. Please wait."));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid world for level streaming"));
        return false;
    }

    // 프레임 제한 설정 (한 프레임에 1ms만 로딩 허용)
    SetFrameLimitedLoading();

    // 현재 레벨이 있다면 안전하게 언로드
    if (CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Log, TEXT("Safely unloading current level before streaming new one"));

        // 델리게이트 정리
        CurrentStreamingLevel->OnLevelLoaded.RemoveDynamic(this, &UCSLevelStreamingSubsystem::OnAsyncLevelLoaded);

        // 레벨 언로드
        CurrentStreamingLevel->SetShouldBeLoaded(false);
        CurrentStreamingLevel->SetShouldBeVisible(false);
        CurrentStreamingLevel = nullptr;

        UE_LOG(LogTemp, Log, TEXT("Current level safely removed"));
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

    UE_LOG(LogTemp, Log, TEXT("Starting FRAME-LIMITED level streaming: %s for C%d_S%d"),
        *ActualLevelPath, ChapterNumber, StageNumber);

    // 비동기 스트리밍 상태 설정
    bIsAsyncStreaming = true;
    PendingChapterNumber = ChapterNumber;
    PendingStageNumber = StageNumber;
    PendingStageData = StageData;

    // 비동기 레벨 스트리밍 생성 (FRAME-LIMITED)
    CurrentStreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
    CurrentStreamingLevel->SetWorldAssetByPackageName(FName(*ActualLevelPath));
    CurrentStreamingLevel->bShouldBlockOnLoad = false;  // 여러 프레임에 걸쳐 분산 처리
    CurrentStreamingLevel->bInitiallyLoaded = true;
    CurrentStreamingLevel->bInitiallyVisible = false;   // 로딩 완료 후 보이게 설정

    // Transform 설정
    CurrentStreamingLevel->LevelTransform = FTransform(FRotator::ZeroRotator, StageData->WorldSpawnPosition);

    // World에 추가
    World->AddStreamingLevel(CurrentStreamingLevel);

    // 비동기 로딩 완료 콜백 바인딩
    CurrentStreamingLevel->OnLevelLoaded.AddDynamic(this, &UCSLevelStreamingSubsystem::OnAsyncLevelLoaded);

    // 비동기 로딩 시작
    CurrentStreamingLevel->SetShouldBeLoaded(true);

    UE_LOG(LogTemp, Log, TEXT("Frame-limited level streaming started (max 1ms per frame) at position: %s"),
        *StageData->WorldSpawnPosition.ToString());

    return true;
}

void UCSLevelStreamingSubsystem::OnAsyncLevelLoaded()
{
    if (!CurrentStreamingLevel || !bIsAsyncStreaming || !PendingStageData)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Async level loaded (frame-limited)! Making visible immediately"));

    // 델리게이트 언바인딩
    CurrentStreamingLevel->OnLevelLoaded.RemoveDynamic(this, &UCSLevelStreamingSubsystem::OnAsyncLevelLoaded);

    // 레벨을 즉시 보이게 설정
    CurrentStreamingLevel->SetShouldBeVisible(true);

    // 기존 코드와 동일한 성공 처리
    CurrentChapter = PendingChapterNumber;
    CurrentStage = PendingStageNumber;
    CurrentSpawnPosition = PendingStageData->WorldSpawnPosition;
    CurrentCharacterSpawnPosition = PendingStageData->CharacterSpawnPosition;

    // Progress subsystem 업데이트
    UCSGameProgressSubsystem* ProgressSubsystem = GetProgressSubsystem();
    if (ProgressSubsystem)
    {
        ProgressSubsystem->SetLastPlayedStage(PendingChapterNumber, PendingStageNumber);
    }

    // 로딩 설정을 일반 모드로 복원 (선택사항)
    // RestoreNormalLoading();

    // 이벤트 브로드캐스트 (기존과 동일)
    OnLevelStreamed.Broadcast(PendingChapterNumber, PendingStageNumber);

    UE_LOG(LogTemp, Log, TEXT("Level streamed successfully (FRAME-LIMITED): %s for C%d_S%d at WorldSpawn:%s, CharacterSpawn:%s"),
        *GetActualLevelPath(PendingStageData), PendingChapterNumber, PendingStageNumber,
        *PendingStageData->WorldSpawnPosition.ToString(), *PendingStageData->CharacterSpawnPosition.ToString());

    // 비동기 상태 초기화
    bIsAsyncStreaming = false;
    PendingChapterNumber = -1;
    PendingStageNumber = -1;
    PendingStageData = nullptr;
}

bool UCSLevelStreamingSubsystem::UnloadCurrentLevel()
{
    if (!CurrentStreamingLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("No current level to unload"));
        return false;
    }

    int32 UnloadedChapter = CurrentChapter;
    int32 UnloadedStage = CurrentStage;

    UE_LOG(LogTemp, Log, TEXT("Safely unloading level C%d_S%d"), UnloadedChapter, UnloadedStage);

    // 델리게이트 정리 (혹시 남아있을 수 있음)
    CurrentStreamingLevel->OnLevelLoaded.RemoveDynamic(this, &UCSLevelStreamingSubsystem::OnAsyncLevelLoaded);

    // 레벨 언로드 (기존 방식 유지)
    CurrentStreamingLevel->SetShouldBeLoaded(false);
    CurrentStreamingLevel->SetShouldBeVisible(false);
    CurrentStreamingLevel = nullptr;

    // 비동기 상태도 초기화
    if (bIsAsyncStreaming)
    {
        bIsAsyncStreaming = false;
        PendingChapterNumber = -1;
        PendingStageNumber = -1;
        PendingStageData = nullptr;
    }

    // 이벤트 브로드캐스트 (기존과 동일)
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

bool UCSLevelStreamingSubsystem::IsAsyncStreamingInProgress() const
{
    return bIsAsyncStreaming;
}

// === 프레임 제한 설정 함수들 ===

void UCSLevelStreamingSubsystem::SetFrameLimitedLoading()
{
    // 한 프레임에 최대 1ms만 로딩 허용
    if (IConsoleVariable* AsyncLoadingTimeLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit")))
    {
        AsyncLoadingTimeLimit->Set(1.0f); // 1ms
        UE_LOG(LogTemp, Log, TEXT("Set AsyncLoadingTimeLimit to 1ms per frame"));
    }

    // 스트리밍 CPU 스로틀링 활성화
    if (IConsoleVariable* ThrottleCPU = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.ThrottleCPU")))
    {
        ThrottleCPU->Set(1); // 활성화
        UE_LOG(LogTemp, Log, TEXT("Enabled CPU throttling for streaming"));
    }

    // 우선순위 로딩 추가 시간 제한
    if (IConsoleVariable* PriorityTime = IConsoleManager::Get().FindConsoleVariable(TEXT("s.PriorityAsyncLoadingExtraTime")))
    {
        PriorityTime->Set(0.5f); // 0.5ms
        UE_LOG(LogTemp, Log, TEXT("Set PriorityAsyncLoadingExtraTime to 0.5ms"));
    }

    // 스트리밍 풀 크기 제한
    if (IConsoleVariable* LimitPoolSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.LimitPoolSize")))
    {
        LimitPoolSize->Set(1); // 제한 활성화
        UE_LOG(LogTemp, Log, TEXT("Enabled streaming pool size limitation"));
    }
}

void UCSLevelStreamingSubsystem::RestoreNormalLoading()
{
    // 기본값으로 복원
    if (IConsoleVariable* AsyncLoadingTimeLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit")))
    {
        AsyncLoadingTimeLimit->Set(5.0f); // 기본값 5ms
        UE_LOG(LogTemp, Log, TEXT("Restored AsyncLoadingTimeLimit to default 5ms"));
    }

    if (IConsoleVariable* ThrottleCPU = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.ThrottleCPU")))
    {
        ThrottleCPU->Set(0); // 비활성화
        UE_LOG(LogTemp, Log, TEXT("Disabled CPU throttling"));
    }
}

UCSGameProgressSubsystem* UCSLevelStreamingSubsystem::GetProgressSubsystem() const
{
    return GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
}