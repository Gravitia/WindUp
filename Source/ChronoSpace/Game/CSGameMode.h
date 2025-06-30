// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Actor/System/CSRespawnPoint.h"
#include "CSGameMode.generated.h"

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

    // Current respawn point
    UPROPERTY(BlueprintReadOnly, Category = "Respawn")
    class ACSRespawnPoint* CurrentRespawnPoint;

public:
    // === Simple Respawn System ===
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void SetCurrentRespawnPoint(class ACSRespawnPoint* NewRespawnPoint);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RespawnAllPlayersAtCurrentPoint();

    // === Player Death Handling (GameState Integration) ===
    UFUNCTION(BlueprintCallable, Category = "Player Death")
    void HandlePlayerDeath(APawn* DeadPlayer);

    // === Events ===
    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnRespawnPointChanged(ACSRespawnPoint* NewRespawnPoint);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnAllPlayersRespawned();

private:
    class ACSGameState* GetCSGameState() const;
    
};
