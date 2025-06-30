// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/CSGameState.h"
#include "Player/CSPlayerState.h"
#include "Game/CSGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"


ACSGameState::ACSGameState()
{
    SetReplicates(true);
    AllDeadRespawnDelay = 2.0f;
}

void ACSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 추가: 플레이어 죽음 상태 복제
    DOREPLIFETIME(ACSGameState, PlayerDeathStates);
}

void ACSGameState::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 초기화
    if (HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("CSGameState initialized on server"));
    }
}

// === 기존: 플레이어 상태 관리 ===
TArray<ACSPlayerState*> ACSGameState::GetAllMyPlayerStates() const
{
    TArray<ACSPlayerState*> MyPlayerStates;
    for (APlayerState* PS : PlayerArray)
    {
        if (ACSPlayerState* MyPS = Cast<ACSPlayerState>(PS))
        {
            MyPlayerStates.Add(MyPS);
        }
    }
    return MyPlayerStates;
}

ACSPlayerState* ACSGameState::GetMyPlayerState(int32 PlayerIndex) const
{
    TArray<ACSPlayerState*> MyPlayerStates = GetAllMyPlayerStates();
    if (MyPlayerStates.IsValidIndex(PlayerIndex))
    {
        return MyPlayerStates[PlayerIndex];
    }
    return nullptr;
}

void ACSGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);
    BroadcastPlayersUpdated();

    // 추가: 플레이어를 죽음 추적에 추가
    if (HasAuthority())
    {
        if (APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner()))
        {
            if (APawn* PlayerPawn = PC->GetPawn())
            {
                AddPlayerToDeathTracking(PlayerPawn);
            }
        }
    }
}

void ACSGameState::RemovePlayerState(APlayerState* PlayerState)
{
    // 추가: 플레이어를 죽음 추적에서 제거
    if (HasAuthority())
    {
        if (APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner()))
        {
            if (APawn* PlayerPawn = PC->GetPawn())
            {
                RemovePlayerFromDeathTracking(PlayerPawn);
            }
        }
    }

    Super::RemovePlayerState(PlayerState);
    BroadcastPlayersUpdated();
}

void ACSGameState::BroadcastPlayersUpdated()
{
    OnPlayersUpdated.Broadcast(GetAllMyPlayerStates());
}

// === 추가: 플레이어 죽음 관리 ===
void ACSGameState::HandlePlayerDeath(APawn* DeadPlayer)
{
    if (!HasAuthority() || !DeadPlayer)
        return;

    FPlayerDeathState* DeathState = FindPlayerDeathState(DeadPlayer);
    if (DeathState && !DeathState->bIsDead)
    {
        DeathState->bIsDead = true;
        DeathState->DeathTime = GetWorld()->GetTimeSeconds();

        // 모든 클라이언트에 알림
        MulticastOnPlayerDied(DeadPlayer);

        UE_LOG(LogTemp, Log, TEXT("Player died: %s"), *DeadPlayer->GetName());

        // 모든 플레이어가 죽었는지 확인
        CheckAllPlayersDeath();
    }
}

void ACSGameState::HandlePlayerRevive(APawn* RevivedPlayer)
{
    if (!HasAuthority() || !RevivedPlayer)
        return;

    FPlayerDeathState* DeathState = FindPlayerDeathState(RevivedPlayer);
    if (DeathState && DeathState->bIsDead)
    {
        DeathState->bIsDead = false;
        DeathState->DeathTime = 0.0f;

        // 리스폰 타이머 클리어
        GetWorld()->GetTimerManager().ClearTimer(AllDeadRespawnTimer);

        // 모든 클라이언트에 알림
        MulticastOnPlayerRevived(RevivedPlayer);

        UE_LOG(LogTemp, Log, TEXT("Player revived: %s"), *RevivedPlayer->GetName());
    }
}

void ACSGameState::AddPlayerToDeathTracking(APawn* Player)
{
    if (!HasAuthority() || !Player)
        return;

    // 이미 추가되어 있는지 확인
    if (!FindPlayerDeathState(Player))
    {
        PlayerDeathStates.Add(FPlayerDeathState(Player));
        UE_LOG(LogTemp, Log, TEXT("Player added to death tracking: %s"), *Player->GetName());
    }
}

void ACSGameState::RemovePlayerFromDeathTracking(APawn* Player)
{
    if (!HasAuthority() || !Player)
        return;

    PlayerDeathStates.RemoveAll([Player](const FPlayerDeathState& State) {
        return State.Player == Player;
        });

    UE_LOG(LogTemp, Log, TEXT("Player removed from death tracking"));
}

bool ACSGameState::AreAllPlayersDead() const
{
    if (PlayerDeathStates.Num() == 0)
        return false;

    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player && !DeathState.bIsDead)
        {
            return false; // 최소 한 명은 살아있음
        }
    }

    return true; // 모든 플레이어가 죽음
}

int32 ACSGameState::GetAlivePlayerCount() const
{
    int32 AliveCount = 0;
    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player && !DeathState.bIsDead)
        {
            AliveCount++;
        }
    }
    return AliveCount;
}

int32 ACSGameState::GetDeadPlayerCount() const
{
    int32 DeadCount = 0;
    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player && DeathState.bIsDead)
        {
            DeadCount++;
        }
    }
    return DeadCount;
}

bool ACSGameState::IsPlayerDead(APawn* Player) const
{
    const FPlayerDeathState* DeathState = FindPlayerDeathState(Player);
    return DeathState ? DeathState->bIsDead : false;
}

TArray<APawn*> ACSGameState::GetAlivePlayers() const
{
    TArray<APawn*> AlivePlayers;
    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player && !DeathState.bIsDead)
        {
            AlivePlayers.Add(DeathState.Player);
        }
    }
    return AlivePlayers;
}

TArray<APawn*> ACSGameState::GetDeadPlayers() const
{
    TArray<APawn*> DeadPlayers;
    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player && DeathState.bIsDead)
        {
            DeadPlayers.Add(DeathState.Player);
        }
    }
    return DeadPlayers;
}

// === 추가: 멀티캐스트 이벤트 구현 ===
void ACSGameState::MulticastOnPlayerDied_Implementation(APawn* DeadPlayer)
{
    OnPlayerDied.Broadcast(DeadPlayer);
}

void ACSGameState::MulticastOnPlayerRevived_Implementation(APawn* RevivedPlayer)
{
    OnPlayerRevived.Broadcast(RevivedPlayer);
}

void ACSGameState::MulticastOnAllPlayersDead_Implementation()
{
    OnAllPlayersDead.Broadcast();
}

void ACSGameState::MulticastOnAllPlayersAboutToRespawn_Implementation(float DelayTime)
{
    OnAllPlayersAboutToRespawn.Broadcast(DelayTime);
}

// === 추가: 내부 헬퍼 함수 ===
FPlayerDeathState* ACSGameState::FindPlayerDeathState(APawn* Player)
{
    for (FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player == Player)
        {
            return &DeathState;
        }
    }
    return nullptr;
}

const FPlayerDeathState* ACSGameState::FindPlayerDeathState(APawn* Player) const
{
    for (const FPlayerDeathState& DeathState : PlayerDeathStates)
    {
        if (DeathState.Player == Player)
        {
            return &DeathState;
        }
    }
    return nullptr;
}

void ACSGameState::CheckAllPlayersDeath()
{
    if (AreAllPlayersDead())
    {
        // 모든 클라이언트에 알림
        MulticastOnAllPlayersDead();
        MulticastOnAllPlayersAboutToRespawn(AllDeadRespawnDelay);

        // 리스폰 타이머 시작
        GetWorld()->GetTimerManager().SetTimer(AllDeadRespawnTimer,
            this,
            &ACSGameState::TriggerAllPlayersRespawn,
            AllDeadRespawnDelay,
            false);

        UE_LOG(LogTemp, Warning, TEXT("All players are dead! Respawning in %.1f seconds"), AllDeadRespawnDelay);
    }
}

void ACSGameState::TriggerAllPlayersRespawn()
{
    // GameMode에게 리스폰 실행 요청
    if (ACSGameMode* GameMode = Cast<ACSGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GameMode->RespawnAllPlayersAtCheckpoint();

        // 모든 플레이어 상태를 살아있음으로 리셋
        for (FPlayerDeathState& DeathState : PlayerDeathStates)
        {
            DeathState.bIsDead = false;
            DeathState.DeathTime = 0.0f;
        }
    }
}

