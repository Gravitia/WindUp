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

    bool RespawnSinglePlayer(APawn* Player);

protected:
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
    /** SeamlessTravel 시, 기존 PlayerController가 옴겨질 때 호출됨 */
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    class ACSGameState* GetCSGameState() const;
    

// Split Screen
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

    UPROPERTY() // GC 보호
    TObjectPtr< class ACSCameraViewProxy > ServerCamProxy = nullptr;

    UPROPERTY()
    TMap< TObjectPtr< APlayerController >, TObjectPtr< ACSCameraViewProxy > > ClientCamProxies;

    TObjectPtr< class ACSSpectatorPawn > DummySpectatorPawn;

private:
    TArray< TObjectPtr< APlayerController > > ConnectedPlayers;
    TObjectPtr< class ACSPlayerController > DummyPlayerController;

    void CreateDummyLocalPlayer();
    void AttachDummySpectatorToClient(APlayerController* RemoteClient);
    void SyncDummyRotationWithProxy();
    void SetupOnlineSplitScreen();

    FTimerHandle SyncTimerHandle;
    FTimerHandle RotationSyncTimerHandle;

};
