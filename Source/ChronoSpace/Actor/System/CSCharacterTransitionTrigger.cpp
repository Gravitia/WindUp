// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/System/CSCharacterTransitionTrigger.h"
#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ACSCharacterTransitionTrigger::ACSCharacterTransitionTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    // 기본 설정
    bCompleteStageOnTransition = true;
    RequiredPlayerCount = 2;            // 기본 2명
    NextChapterNumber = 1;
    NextStageNumber = 1;
    TransitionDelay = 0.5f;             // 0.5초 대기
    bLevelStreamingStarted = false;
    bCharacterMoveCompleted = false;

    // 오버랩 이벤트 바인딩
    GetCollisionComponent()->OnComponentBeginOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapBegin);
    GetCollisionComponent()->OnComponentEndOverlap.AddDynamic(this, &ACSCharacterTransitionTrigger::OnOverlapEnd);

    // 네트워킹 설정
    bReplicates = true;
    SetReplicateMovement(false);  // 트리거는 이동하지 않으므로
}

void ACSCharacterTransitionTrigger::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicated 변수들 등록
    DOREPLIFETIME(ACSCharacterTransitionTrigger, PlayersInTrigger);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, bLevelStreamingStarted);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, bCharacterMoveCompleted);
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
    // 서버에서만 트리거 로직 실행
    if (!HasAuthority())
    {
        return;
    }

    // 이미 캐릭터 이동이 완료되었으면 무시
    if (bCharacterMoveCompleted)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character || PlayersInTrigger.Contains(Character))
    {
        return;
    }

    PlayersInTrigger.Add(Character);
    UE_LOG(LogTemp, Log, TEXT("Character entered trigger. Current count: %d/%d"),
        PlayersInTrigger.Num(), RequiredPlayerCount);

    // 모든 클라이언트에 상태 업데이트
    MulticastUpdateTriggerState(PlayersInTrigger.Num(), bLevelStreamingStarted, bCharacterMoveCompleted);

    // 트리거 조건 확인
    CheckTriggerConditions();
}

void ACSCharacterTransitionTrigger::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 서버에서만 트리거 로직 실행
    if (!HasAuthority())
    {
        return;
    }

    // 이미 캐릭터 이동이 완료되었으면 무시
    if (bCharacterMoveCompleted)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character)
    {
        PlayersInTrigger.Remove(Character);
        UE_LOG(LogTemp, Log, TEXT("Character left trigger. Current count: %d/%d"),
            PlayersInTrigger.Num(), RequiredPlayerCount);

        // 모든 클라이언트에 상태 업데이트
        MulticastUpdateTriggerState(PlayersInTrigger.Num(), bLevelStreamingStarted, bCharacterMoveCompleted);
    }
}

void ACSCharacterTransitionTrigger::MulticastTransitionToNextStage_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("MulticastTransitionToNextStage called on %s"),
        HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void ACSCharacterTransitionTrigger::MulticastMoveCharactersToNewPosition_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("MulticastMoveCharactersToNewPosition called on %s"),
        HasAuthority() ? TEXT("Server") : TEXT("Client"));

    // 각 클라이언트에서 지연 후 캐릭터 이동 실행
    FTimerHandle MoveTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(MoveTimerHandle, this,
        &ACSCharacterTransitionTrigger::MoveLocalCharacterToNewPosition, TransitionDelay, false);
}

void ACSCharacterTransitionTrigger::MulticastUpdateTriggerState_Implementation(int32 PlayerCount, bool bStreamingStarted, bool bMoveCompleted)
{
    // 클라이언트에서 UI 업데이트 등에 사용할 수 있는 함수
    UE_LOG(LogTemp, Log, TEXT("Trigger state updated - Players: %d, Streaming: %s, Move: %s"),
        PlayerCount, bStreamingStarted ? TEXT("Started") : TEXT("Not Started"),
        bMoveCompleted ? TEXT("Completed") : TEXT("Not Completed"));
}

void ACSCharacterTransitionTrigger::CheckTriggerConditions()
{
    // 조건 1: 첫 번째 캐릭터가 들어왔을 때 레벨 스트리밍 시작
    if (!bLevelStreamingStarted && PlayersInTrigger.Num() >= 1)
    {
        UE_LOG(LogTemp, Log, TEXT("First player entered - starting level streaming for C%d_S%d"),
            NextChapterNumber, NextStageNumber);
        ServerStartLevelStreaming();
    }

    // 조건 2: 모든 플레이어가 들어왔을 때 캐릭터 이동
    if (bLevelStreamingStarted && !bCharacterMoveCompleted && PlayersInTrigger.Num() >= RequiredPlayerCount)
    {
        UE_LOG(LogTemp, Log, TEXT("All %d players in trigger - starting character movement"),
            RequiredPlayerCount);
        ServerMoveCharacters();
    }
}

void ACSCharacterTransitionTrigger::ServerStartLevelStreaming()
{
    if (!HasAuthority() || bLevelStreamingStarted)
    {
        return;
    }

    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get CSLevelStreamingSubsystem on server"));
        return;
    }

    // 레벨 스트리밍 시작
    bool bStreamSuccess = LevelSubsystem->StreamLevel(NextChapterNumber, NextStageNumber);
    if (bStreamSuccess)
    {
        bLevelStreamingStarted = true;
        UE_LOG(LogTemp, Log, TEXT("Level streaming started successfully for C%d_S%d"),
            NextChapterNumber, NextStageNumber);

        // 스테이지 완료 처리
        if (bCompleteStageOnTransition)
        {
            LevelSubsystem->CompleteCurrentStage();
        }

        // 모든 클라이언트에 레벨 전환 알림
        MulticastTransitionToNextStage();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to start level streaming for C%d_S%d"),
            NextChapterNumber, NextStageNumber);
    }
}

void ACSCharacterTransitionTrigger::ServerMoveCharacters()
{
    if (!HasAuthority() || bCharacterMoveCompleted)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("All players ready - starting character movement"));

    bCharacterMoveCompleted = true;

    // 모든 클라이언트에서 캐릭터 이동 실행
    MulticastMoveCharactersToNewPosition();
}

void ACSCharacterTransitionTrigger::MoveLocalCharacterToNewPosition()
{
    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get LevelStreamingSubsystem during character movement"));
        return;
    }

    FVector NewPosition = LevelSubsystem->GetSpawnPosition();

    // 각 클라이언트에서 자신이 소유한 캐릭터만 이동
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    APawn* PlayerPawn = PC->GetPawn();
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn);

    if (PlayerCharacter)
    {
        PlayerCharacter->SetActorLocation(NewPosition);
        UE_LOG(LogTemp, Log, TEXT("Local character moved to position: %s on %s"),
            *NewPosition.ToString(), HasAuthority() ? TEXT("Server") : TEXT("Client"));
    }

    // 서버에서만 정리 작업
    if (HasAuthority())
    {
        PlayersInTrigger.Empty();
        UE_LOG(LogTemp, Log, TEXT("Character transition completed on server"));

        // 필요시 일정 시간 후 트리거 재활성화
        // FTimerHandle ResetHandle;
        // GetWorld()->GetTimerManager().SetTimer(ResetHandle, [this]() { 
        //     bLevelStreamingStarted = false; 
        //     bCharacterMoveCompleted = false; 
        // }, 10.0f, false);
    }
}

void ACSCharacterTransitionTrigger::SetNextStage(int32 ChapterNumber, int32 StageNumber)
{
    NextChapterNumber = ChapterNumber;
    NextStageNumber = StageNumber;

    UE_LOG(LogTemp, Log, TEXT("Next stage set to: C%d_S%d"), NextChapterNumber, NextStageNumber);
}

void ACSCharacterTransitionTrigger::SetRequiredPlayerCount(int32 Count)
{
    RequiredPlayerCount = FMath::Clamp(Count, 1, 4);  // 1~4명으로 제한
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

UCSLevelStreamingSubsystem* ACSCharacterTransitionTrigger::GetLevelStreamingSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSLevelStreamingSubsystem>();
    }
    return nullptr;
}

UCSGameProgressSubsystem* ACSCharacterTransitionTrigger::GetProgressSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSGameProgressSubsystem>();
    }
    return nullptr;
}