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
    DOREPLIFETIME(ACSCharacterTransitionTrigger, NextChapterNumber);
    DOREPLIFETIME(ACSCharacterTransitionTrigger, NextStageNumber);
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

    //  간단하게 수정 - 복잡한 분기 제거
    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (LevelSubsystem)
    {
        // 모든 머신에서 동일하게 RequestStreamLevel 호출
        LevelSubsystem->RequestStreamLevel(NextChapterNumber, NextStageNumber);
        UE_LOG(LogTemp, Log, TEXT("Requested level streaming for C%d_S%d on %s"),
            NextChapterNumber, NextStageNumber, HasAuthority() ? TEXT("Server") : TEXT("Client"));

        // 스테이지 완료 처리 (서버에서만)
        if (HasAuthority() && bCompleteStageOnTransition)
        {
            LevelSubsystem->CompleteCurrentStage();
        }
    }
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
    if (bLevelStreamingStarted)
    {
        return;
    }

    // 레벨 스트리밍 상태 업데이트
    bLevelStreamingStarted = true;

    UE_LOG(LogTemp, Log, TEXT("Starting level streaming process for C%d_S%d"),
        NextChapterNumber, NextStageNumber);

    // 모든 클라이언트에서 레벨 스트리밍 실행
    MulticastTransitionToNextStage();
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

    // 캐릭터 스폰 위치 사용 (WorldSpawnPosition 대신)
    FVector NewPosition = LevelSubsystem->GetCharacterSpawnPosition();

    // 모든 머신에서 캐릭터 이동 처리
    UWorld* World = GetWorld();
    if (!World) return;

    // 서버와 클라이언트 모두에서 처리
    if (HasAuthority())
    {
        // 서버: 모든 플레이어 캐릭터 이동
        for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            APlayerController* PC = Iterator->Get();
            if (PC && PC->GetPawn())
            {
                ACharacter* PlayerCharacter = Cast<ACharacter>(PC->GetPawn());
                if (PlayerCharacter)
                {
                    PlayerCharacter->SetActorLocation(NewPosition);
                    UE_LOG(LogTemp, Log, TEXT("Server moved character to CharacterSpawnPosition: %s"), *NewPosition.ToString());
                }
            }
        }

        // 서버 정리 작업
        PlayersInTrigger.Empty();
        UE_LOG(LogTemp, Log, TEXT("Character transition completed on server"));
    }
    else
    {
        // 클라이언트: 자신의 캐릭터만 이동 (추가 보장)
        APlayerController* PC = World->GetFirstPlayerController();
        if (PC && PC->GetPawn())
        {
            ACharacter* PlayerCharacter = Cast<ACharacter>(PC->GetPawn());
            if (PlayerCharacter)
            {
                PlayerCharacter->SetActorLocation(NewPosition);
                UE_LOG(LogTemp, Log, TEXT("Client moved local character to CharacterSpawnPosition: %s"), *NewPosition.ToString());
            }
        }
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