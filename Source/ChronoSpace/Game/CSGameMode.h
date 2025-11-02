// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Actor/CSCameraViewProxy.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "CSGameState.h"
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

    // === Events ===
    UPROPERTY(BlueprintAssignable, Category = "GameMode Events")
    FOnPlayerLogin OnPlayerLogin;

    // === Pawn Classes ===
    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer0;

    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer1;

    // === Split Screen Configuration ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

    // SetViewTarget 옵션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|ViewTarget")
    bool bCreateDummySpectatorPawn = false;  // SetViewTarget에서는 선택사항

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|ViewTarget")
    bool bUseViewTargetValidation = true;  // ViewTarget 유효성 주기적 체크

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|ViewTarget")
    bool bUseCameraProxySync = false;  // 추가 카메라 동기화 필요시

protected:
    // === Core Overrides ===
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    // === Respawn System ===
    UPROPERTY(BlueprintReadOnly, Category = "Respawn")
    ACSRespawnPoint* CurrentRespawnPoint;

public:
    // === Respawn Functions ===
    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void SetCurrentRespawnPoint(ACSRespawnPoint* NewRespawnPoint);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    void RespawnAllPlayersAtCurrentPoint();

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool RespawnSinglePlayer(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Respawn")
    bool RespawnPlayerAtCurrentPoint(APawn* Player);

    // === Player Death Handling ===
    UFUNCTION(BlueprintCallable, Category = "Player Death")
    void HandlePlayerDeath(APawn* DeadPlayer);

    // === Blueprint Events ===
    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnRespawnPointChanged(ACSRespawnPoint* NewRespawnPoint);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMode Events")
    void OnAllPlayersRespawned();

protected:
    // === Split Screen Implementation (SetViewTarget) ===
    void SetupOnlineSplitScreen();
    void CreateDummyLocalPlayer();

    // ViewTarget 관리
    UFUNCTION()
    void ValidateAndUpdateViewTarget();

    // 선택적 카메라 프록시 동기화
    UFUNCTION()
    void SyncDummyCameraWithProxy();

    // Deprecated (하위 호환성)
    UFUNCTION()
    void AttachDummySpectatorToClient(APlayerController* RemoteClient);

    UFUNCTION()
    void SyncDummyRotationWithProxy();

private:
    // === Internal Helpers ===
    ACSGameState* GetCSGameState() const;

protected:
    // === Camera Proxy System ===
    UPROPERTY()
    TObjectPtr<ACSCameraViewProxy> ServerCamProxy = nullptr;

    UPROPERTY()
    TMap<TObjectPtr<APlayerController>, TObjectPtr<ACSCameraViewProxy>> ClientCamProxies;

private:
    // === Runtime Data ===
    TArray<TObjectPtr<APlayerController>> ConnectedPlayers;

    UPROPERTY()
    TObjectPtr<ACSSpectatorPawn> DummySpectatorPawn;

    UPROPERTY()
    TObjectPtr<ACSPlayerController> DummyPlayerController;

    // === Timers ===
    FTimerHandle ViewTargetValidationTimer;  // ViewTarget 유효성 체크
    FTimerHandle CameraProxySyncTimer;       // 선택적 카메라 동기화

    // Deprecated timers (하위 호환성)
    FTimerHandle SyncTimerHandle;
    FTimerHandle RotationSyncTimerHandle;

public:
    // === Getters (Blueprint accessible) ===
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    ACSPlayerController* GetDummyPlayerController() const { return DummyPlayerController; }

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    ACSSpectatorPawn* GetDummySpectatorPawn() const { return DummySpectatorPawn; }

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    int32 GetConnectedPlayerCount() const { return ConnectedPlayers.Num(); }

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    bool IsSplitScreenActive() const { return DummyPlayerController != nullptr; }
};
