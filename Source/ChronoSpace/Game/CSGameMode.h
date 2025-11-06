// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Actor/System/CSRespawnPoint.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "CSGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerLogin);

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    ACSGameMode();
    FOnPlayerLogin OnPlayerLogin;

    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer0;

    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer1;

protected:
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

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

    // === Single Player Respawn ===
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool RespawnSinglePlayer(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool RespawnPlayerAtCurrentPoint(APawn* Player);

private:
    class ACSGameState* GetCSGameState() const;
    

// Split Screen
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

    UPROPERTY() // GC º¸È£
    TObjectPtr< class ACSCameraViewProxy > ServerCamProxy = nullptr;

    UPROPERTY()
    TMap< TObjectPtr< APlayerController >, TObjectPtr< ACSCameraViewProxy > > ClientCamProxies;
    void SetupOnlineSplitScreen();

private:
    TArray< TObjectPtr< APlayerController > > ConnectedPlayers;
    TObjectPtr< class ACSSpectatorPawn > DummySpectatorPawn;
    TObjectPtr< class ACSPlayerController > DummyPlayerController;

    void CreateDummyLocalPlayer();
    void AttachDummySpectatorToClient(APlayerController* RemoteClient);
    void SyncDummyRotationWithProxy();
    

    FTimerHandle SyncTimerHandle;
    FTimerHandle RotationSyncTimerHandle;
};
