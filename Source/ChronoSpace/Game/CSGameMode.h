// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "CSGameMode.generated.h"

UENUM(BlueprintType)
enum class ERespawnType : uint8
{
    KillZone,      // 떨어져서 리스폰 포인트로 이동.
    AllPlayersDead,
    LoadFromSave,       // 저장된 게임에서 로드
    Manual              // 수동 리스폰
};

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSGameMode : public AGameModeBase
{
	GENERATED_BODY()
	

public:
    ACSGameMode();
    
protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // Current active checkpoint
    UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
    FString CurrentActiveCheckpointID;

    // Level actors
    UPROPERTY(BlueprintReadOnly, Category = "Level Actors")
    TArray<class ACSCheckPoint*> LevelCheckpoints;

    UPROPERTY(BlueprintReadOnly, Category = "Level Actors")
    TArray<class ACSRespawnPoint*> LevelRespawnPoints;

    // Checkpoint to RespawnPoints mapping
    UPROPERTY()
    TMap<FString, TArray<class ACSRespawnPoint*>> CheckpointRespawnMap;

public:
    // === Player Death Handling (GameState Integration) ===
    UFUNCTION(BlueprintCallable, Category = "Player Death")
    void HandlePlayerDeath(APawn* DeadPlayer);

    // === Respawn Functions ===
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RespawnAllPlayersAtCheckpoint();

    // === Checkpoint Management ===
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void RegisterCheckpoint(ACSCheckPoint* Checkpoint);

    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void ActivateCheckpoint(const FString& CheckpointID);

    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    ACSCheckPoint* FindCheckpointByID(const FString& CheckpointID) const;

    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    FString GetCurrentActiveCheckpointID() const { return CurrentActiveCheckpointID; }

    // === RespawnPoint Management ===
    UFUNCTION(BlueprintCallable, Category = "RespawnPoint")
    void RegisterRespawnPoint(ACSRespawnPoint* RespawnPoint);

    UFUNCTION(BlueprintCallable, Category = "RespawnPoint")
    ACSRespawnPoint* GetBestRespawnPoint(const FString& CheckpointID) const;

    UFUNCTION(BlueprintCallable, Category = "RespawnPoint")
    TArray<ACSRespawnPoint*> GetRespawnPointsForCheckpoint(const FString& CheckpointID) const;

    // === Events ===
    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnCheckpointActivated(const FString& CheckpointID);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnAllPlayersRespawned();

private:
    void InitializeLevelActors();
    void BuildCheckpointRespawnMap();
    void SpawnPlayerAtRespawnPoint(APawn* Player, ACSRespawnPoint* RespawnPoint);
    void DeactivateAllCheckpoints();
    class ACSGameState* GetCSGameState() const;

    
};
