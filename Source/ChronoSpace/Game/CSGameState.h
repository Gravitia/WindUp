// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "CSGameState.generated.h"

class ACSPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayersUpdated, const TArray<ACSPlayerState*>&, PlayerStates);

USTRUCT(BlueprintType)
struct FPlayerDeathState
{
	GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)  // Replicated 제거
    class APawn* Player;

    UPROPERTY(BlueprintReadOnly)  // Replicated 제거
    bool bIsDead;

    UPROPERTY(BlueprintReadOnly)  // Replicated 제거
    float DeathTime;

    FPlayerDeathState()
    {
        Player = nullptr;
        bIsDead = false;
        DeathTime = 0.0f;
    }

    FPlayerDeathState(APawn* InPlayer)
    {
        Player = InPlayer;
        bIsDead = false;
        DeathTime = 0.0f;
    }
};
/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSGameState : public AGameStateBase
{
	GENERATED_BODY() 

public:
    ACSGameState();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

    // === 플레이어 죽음 상태 관리 ===
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Death")
    TArray<FPlayerDeathState> PlayerDeathStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn Settings")
    float AllDeadRespawnDelay = 2.0f;

    FTimerHandle AllDeadRespawnTimer;

public:
    // === 기존: 플레이어 상태 관리 ===
    UFUNCTION(BlueprintCallable, Category = "Players")
    TArray<ACSPlayerState*> GetAllMyPlayerStates() const;

    UFUNCTION(BlueprintCallable, Category = "Players")
    ACSPlayerState* GetMyPlayerState(int32 PlayerIndex) const;

    // === 플레이어 죽음 관리 ===
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Death")
    void HandlePlayerDeath(APawn* DeadPlayer);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Death")
    void HandlePlayerRevive(APawn* RevivedPlayer);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Death")
    void AddPlayerToDeathTracking(APawn* Player);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player Death")
    void RemovePlayerFromDeathTracking(APawn* Player);

    // === 플레이어 상태 조회 ===
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    bool AreAllPlayersDead() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    int32 GetAlivePlayerCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    int32 GetDeadPlayerCount() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    bool IsPlayerDead(APawn* Player) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    TArray<APawn*> GetAlivePlayers() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player State")
    TArray<APawn*> GetDeadPlayers() const;

    // === 추가: Level Streaming RPC Functions ===
    // 클라이언트에서 서버로 레벨 스트리밍 요청
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Level Streaming RPC")
    void ServerStreamLevel(int32 ChapterNumber, int32 StageNumber);

    // 클라이언트에서 서버로 레벨 언로드 요청
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Level Streaming RPC")
    void ServerUnloadCurrentLevel();

    // 서버에서 모든 클라이언트로 레벨 스트리밍 완료 알림
    UFUNCTION(NetMulticast, Reliable, Category = "Level Streaming RPC")
    void MulticastOnLevelStreamed(int32 ChapterNumber, int32 StageNumber, const FVector& SpawnPosition);

    // 서버에서 모든 클라이언트로 레벨 언로드 완료 알림
    UFUNCTION(NetMulticast, Reliable, Category = "Level Streaming RPC")
    void MulticastOnLevelUnloaded(int32 ChapterNumber, int32 StageNumber);

    // === 델리게이트 ===
    UPROPERTY(BlueprintAssignable, Category = "Players")
    FOnPlayersUpdated OnPlayersUpdated;

    // === 플레이어 죽음 이벤트 ===
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDied, APawn*, DeadPlayer);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRevived, APawn*, RevivedPlayer);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllPlayersDead);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllPlayersAboutToRespawn, float, DelayTime);

    UPROPERTY(BlueprintAssignable, Category = "Player Death Events")
    FOnPlayerDied OnPlayerDied;

    UPROPERTY(BlueprintAssignable, Category = "Player Death Events")
    FOnPlayerRevived OnPlayerRevived;

    UPROPERTY(BlueprintAssignable, Category = "Player Death Events")
    FOnAllPlayersDead OnAllPlayersDead;

    UPROPERTY(BlueprintAssignable, Category = "Player Death Events")
    FOnAllPlayersAboutToRespawn OnAllPlayersAboutToRespawn;

protected:
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    // === 플레이어 관련 멀티캐스트 이벤트 ===
    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnPlayerDied(APawn* DeadPlayer);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnPlayerRevived(APawn* RevivedPlayer);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnAllPlayersDead();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnAllPlayersAboutToRespawn(float DelayTime);

    // === Level Streaming RPC 구현 함수들 ===
    void ServerStreamLevel_Implementation(int32 ChapterNumber, int32 StageNumber);
    void ServerUnloadCurrentLevel_Implementation();
    void MulticastOnLevelStreamed_Implementation(int32 ChapterNumber, int32 StageNumber, const FVector& SpawnPosition);
    void MulticastOnLevelUnloaded_Implementation(int32 ChapterNumber, int32 StageNumber);

private:
    void BroadcastPlayersUpdated();
    FPlayerDeathState* FindPlayerDeathState(APawn* Player);
    const FPlayerDeathState* FindPlayerDeathState(APawn* Player) const;
    void CheckAllPlayersDeath();
    void TriggerAllPlayersRespawn();

    // Level Streaming Subsystem 참조 헬퍼
    class UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;
};
