// ACSCharacterTransitionTrigger.cpp

#include "Actor/System/CSCharacterTransitionTrigger.h"
#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ACSCharacterTransitionTrigger::ACSCharacterTransitionTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    bCompleteStageOnTransition = true;
    RequiredPlayerCount = 2;
    NextChapterNumber = 1;
    NextStageNumber = 1;
    TransitionDelay = 0.5f;
    bLevelStreamingStarted = false;
    bCharacterMoveCompleted = false;

    CurrentStreamingLevel = nullptr;
    bIsAsyncStreaming = false;
    PendingChapterNumber = -1;
    PendingStageNumber = -1;
    PendingStageData = nullptr;

    GetCollisionComponent()->OnComponentBeginOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapBegin);
    GetCollisionComponent()->OnComponentEndOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapEnd);

    bReplicates = true;
    SetReplicateMovement(false);
}

void ACSCharacterTransitionTrigger::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSCharacterTransitionTrigger, NextChapterNumber);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, NextStageNumber);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, bLevelStreamingStarted);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, bCharacterMoveCompleted);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, CurrentChapter);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, CurrentStage);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, CurrentSpawnPosition);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, CurrentCharacterSpawnPosition);
}

void ACSCharacterTransitionTrigger::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("CSCharacterTransitionTrigger initialized - Next Stage: C%d_S%d, Required Players: %d"),
        NextChapterNumber, NextStageNumber, RequiredPlayerCount);
}

void ACSCharacterTransitionTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || bCharacterMoveCompleted) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character || PlayersInTrigger.Contains(Character)) return;

    PlayersInTrigger.Add(Character);
    UE_LOG(LogTemp, Log, TEXT("Character entered trigger. Current count: %d/%d"),
        PlayersInTrigger.Num(), RequiredPlayerCount);

    MulticastUpdateTriggerState(PlayersInTrigger.Num(), bLevelStreamingStarted, bCharacterMoveCompleted);
    CheckTriggerConditions();
}

void ACSCharacterTransitionTrigger::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority() || bCharacterMoveCompleted) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character)
    {
        PlayersInTrigger.Remove(Character);
        UE_LOG(LogTemp, Log, TEXT("Character left trigger. Current count: %d/%d"),
            PlayersInTrigger.Num(), RequiredPlayerCount);

        MulticastUpdateTriggerState(PlayersInTrigger.Num(), bLevelStreamingStarted, bCharacterMoveCompleted);
    }
}

void ACSCharacterTransitionTrigger::CheckTriggerConditions()
{
    if (!bLevelStreamingStarted && PlayersInTrigger.Num() >= 1)
    {
        UE_LOG(LogTemp, Log, TEXT("First player entered - starting level streaming for C%d_S%d"),
            NextChapterNumber, NextStageNumber);
        ServerStartLevelStreaming();
    }

    if (bLevelStreamingStarted && !bCharacterMoveCompleted && PlayersInTrigger.Num() >= RequiredPlayerCount)
    {
        UE_LOG(LogTemp, Log, TEXT("All %d players in trigger - starting character movement"),
            RequiredPlayerCount);
        ServerMoveCharacters();
    }
}

void ACSCharacterTransitionTrigger::ServerStartLevelStreaming_Implementation()
{
    if (bLevelStreamingStarted) return;
    bLevelStreamingStarted = true;

    UE_LOG(LogTemp, Log, TEXT("SERVER: Starting level streaming process for C%d_S%d"),
        NextChapterNumber, NextStageNumber);
    MulticastUpdateTriggerState(PlayersInTrigger.Num(), bLevelStreamingStarted, bCharacterMoveCompleted);
    // 먼저 클라이언트에게도 스트리밍 시작 호출
    MulticastStartLevelStreaming(NextChapterNumber, NextStageNumber);

    // 서버 자체 스트리밍
    bool bSuccess = PerformLevelStreaming(NextChapterNumber, NextStageNumber);
    if (bSuccess && bCompleteStageOnTransition)
    {
        if (auto* LevelSubsystem = GetLevelStreamingSubsystem())
            LevelSubsystem->CompleteCurrentStage();
    }
    else if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start level streaming for C%d_S%d"), NextChapterNumber, NextStageNumber);
        bLevelStreamingStarted = false;
    }
}

void ACSCharacterTransitionTrigger::MulticastStartLevelStreaming_Implementation(int32 ChapterNumber, int32 StageNumber)
{
    // 클라이언트도 로딩 로직을 그대로 실행
    if (!HasAuthority())  // 서버는 이미 실행했으므로 중복 방지
    {
        PerformLevelStreaming(ChapterNumber, StageNumber);
    }
}
bool ACSCharacterTransitionTrigger::PerformLevelStreaming(int32 ChapterNumber, int32 StageNumber)
{
    if (bIsAsyncStreaming)
        return false;

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid world"));
        return false;
    }

    auto* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("No LevelSubsystem"));
        return false;
    }

    FStageData* StageData = LevelSubsystem->GetStageDataFromTable(ChapterNumber, StageNumber);
    if (!StageData)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid stage data"));
        return false;
    }

    const FString& LevelToLoad = LevelSubsystem->GetActualLevelPath(StageData);

    // Tune async loading so each frame spends at most 1ms
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit")))
    {
        CVar->Set(1.0f);
        UE_LOG(LogTemp, Log, TEXT("Set s.AsyncLoadingTimeLimit to 1ms"));
    }
    // Enable CPU throttling for streaming
    if (IConsoleVariable* CVar2 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.ThrottleCPU")))
    {
        CVar2->Set(1);
        UE_LOG(LogTemp, Log, TEXT("Enabled r.Streaming.ThrottleCPU"));
    }
    // Add extra time to priority async loading
    if (IConsoleVariable* CVar3 = IConsoleManager::Get().FindConsoleVariable(TEXT("s.PriorityAsyncLoadingExtraTime")))
    {
        CVar3->Set(0.5f);
        UE_LOG(LogTemp, Log, TEXT("Set s.PriorityAsyncLoadingExtraTime to 0.5ms"));
    }
    // Limit streaming pool size
    if (IConsoleVariable* CVar4 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.LimitPoolSize")))
    {
        CVar4->Set(1);
        UE_LOG(LogTemp, Log, TEXT("Enabled r.Streaming.LimitPoolSize"));
    }

    bool bOutSuccess = false;
    ULevelStreamingDynamic* StreamLevel = ULevelStreamingDynamic::LoadLevelInstance(
        World,
        LevelToLoad,
        StageData->WorldSpawnPosition,
        FRotator::ZeroRotator,
        bOutSuccess
    );

    if (!bOutSuccess || !StreamLevel)
    {
        UE_LOG(LogTemp, Error, TEXT("LoadLevelInstance failed: %s"), *LevelToLoad);
        return false;
    }

    StreamLevel->OnLevelLoaded.AddDynamic(this, &ACSCharacterTransitionTrigger::OnAsyncLevelLoaded);

    bIsAsyncStreaming = true;
    PendingChapterNumber = ChapterNumber;
    PendingStageNumber = StageNumber;
    PendingStageData = StageData;

    UE_LOG(LogTemp, Log, TEXT("Started async streaming via LoadLevelInstance: %s"), *LevelToLoad);
    return true;
}


void ACSCharacterTransitionTrigger::OnAsyncLevelLoaded()
{
    if (!CurrentStreamingLevel || !bIsAsyncStreaming || !PendingStageData) return;

    CurrentStreamingLevel->OnLevelLoaded.RemoveDynamic(this, &ACSCharacterTransitionTrigger::OnAsyncLevelLoaded);
    CurrentStreamingLevel->SetShouldBeVisible(true);

    CurrentChapter = PendingChapterNumber;
    CurrentStage = PendingStageNumber;
    CurrentSpawnPosition = PendingStageData->WorldSpawnPosition;
    CurrentCharacterSpawnPosition = PendingStageData->CharacterSpawnPosition;

    if (auto* ProgressSubsystem = GetProgressSubsystem())
        ProgressSubsystem->SetLastPlayedStage(PendingChapterNumber, PendingStageNumber);

    ClientOnLevelStreamingCompleted(PendingChapterNumber, PendingStageNumber,
        PendingStageData->WorldSpawnPosition, PendingStageData->CharacterSpawnPosition);

    // 
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit")))
        CVar->Set(5.0f);  // 

    if (IConsoleVariable* CVar2 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.ThrottleCPU")))
        CVar2->Set(0);    // 

    if (IConsoleVariable* CVar3 = IConsoleManager::Get().FindConsoleVariable(TEXT("s.PriorityAsyncLoadingExtraTime")))
        CVar3->Set(0.0f);

    if (IConsoleVariable* CVar4 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.LimitPoolSize")))
        CVar4->Set(0);
    // 

    bIsAsyncStreaming = false;
    PendingChapterNumber = -1;
    PendingStageNumber = -1;
    PendingStageData = nullptr;
}

void ACSCharacterTransitionTrigger::ClientOnLevelStreamingCompleted_Implementation(int32 ChapterNumber, int32 StageNumber,
    FVector WorldSpawn, FVector CharacterSpawn)
{
    UE_LOG(LogTemp, Log, TEXT("CLIENT: Level streaming completed C%d_S%d"), ChapterNumber, StageNumber);

    CurrentChapter = ChapterNumber;
    CurrentStage = StageNumber;
    CurrentSpawnPosition = WorldSpawn;
    CurrentCharacterSpawnPosition = CharacterSpawn;

    if (auto* LevelSubsystem = GetLevelStreamingSubsystem())
        LevelSubsystem->OnLevelStreamed.Broadcast(ChapterNumber, StageNumber);
}

void ACSCharacterTransitionTrigger::ServerMoveCharacters_Implementation()
{
    if (bCharacterMoveCompleted) return;

    bCharacterMoveCompleted = true;
    MulticastMoveCharactersToNewPosition();
}

void ACSCharacterTransitionTrigger::MulticastMoveCharactersToNewPosition_Implementation()
{
    FTimerHandle MoveTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(MoveTimerHandle, this,
        &ACSCharacterTransitionTrigger::MoveLocalCharacterToNewPosition, TransitionDelay, false);
}

void ACSCharacterTransitionTrigger::MulticastUpdateTriggerState_Implementation(int32 PlayerCount, bool bStreamingStarted, bool bMoveCompleted)
{
    UE_LOG(LogTemp, Log, TEXT("Trigger state - Players:%d, Streaming:%s, Move:%s"),
        PlayerCount,
        bStreamingStarted ? TEXT("Started") : TEXT("Not"),
        bMoveCompleted ? TEXT("Done") : TEXT("Not"));
}

void ACSCharacterTransitionTrigger::MoveLocalCharacterToNewPosition()
{
    FVector NewPos = CurrentCharacterSpawnPosition;
    UWorld* World = GetWorld();
    if (!World) return;

    if (HasAuthority())
    {
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (auto* PC = It->Get(); PC && PC->GetPawn())
            {
                PC->GetPawn()->SetActorLocation(NewPos);
            }
        }
        PlayersInTrigger.Empty();
        UE_LOG(LogTemp, Log, TEXT("Server moved all characters"));
    }
    else
    {
        if (auto* PC = World->GetFirstPlayerController(); PC && PC->GetPawn())
        {
            PC->GetPawn()->SetActorLocation(NewPos);
            UE_LOG(LogTemp, Log, TEXT("Client moved local character"));
        }
    }
}

// --- Getter / Setter / Subsystem ---
void ACSCharacterTransitionTrigger::SetNextStage(int32 ChapterNumber, int32 StageNumber)
{
    NextChapterNumber = ChapterNumber;
    NextStageNumber = StageNumber;
    UE_LOG(LogTemp, Log, TEXT("Next stage set to: C%d_S%d"), ChapterNumber, StageNumber);
}

void ACSCharacterTransitionTrigger::SetRequiredPlayerCount(int32 Count)
{
    RequiredPlayerCount = FMath::Clamp(Count, 1, 4);
    UE_LOG(LogTemp, Log, TEXT("Required player count set to: %d"), RequiredPlayerCount);
}

bool ACSCharacterTransitionTrigger::AreAllPlayersReady() const
{
    return PlayersInTrigger.Num() >= RequiredPlayerCount;
}

int32 ACSCharacterTransitionTrigger::GetCurrentPlayerCount() const
{
    return PlayersInTrigger.Num();
}

bool ACSCharacterTransitionTrigger::IsLevelStreamingStarted() const
{
    return bLevelStreamingStarted;
}

bool ACSCharacterTransitionTrigger::IsCharacterMoveCompleted() const
{
    return bCharacterMoveCompleted;
}

FVector ACSCharacterTransitionTrigger::GetCurrentSpawnPosition() const
{
    return CurrentSpawnPosition;
}

FVector ACSCharacterTransitionTrigger::GetCurrentCharacterSpawnPosition() const
{
    return CurrentCharacterSpawnPosition;
}

void ACSCharacterTransitionTrigger::GetCurrentStage(int32& OutChapter, int32& OutStage) const
{
    OutChapter = CurrentChapter;
    OutStage = CurrentStage;
}

UCSLevelStreamingSubsystem* ACSCharacterTransitionTrigger::GetLevelStreamingSubsystem() const
{
    if (auto* GI = GetWorld()->GetGameInstance())
        return GI->GetSubsystem<UCSLevelStreamingSubsystem>();
    return nullptr;
}

UCSGameProgressSubsystem* ACSCharacterTransitionTrigger::GetProgressSubsystem() const
{
    if (auto* GI = GetWorld()->GetGameInstance())
        return GI->GetSubsystem<UCSGameProgressSubsystem>();
    return nullptr;
}
