// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/CSGameMode.h"
#include "Game/CSGameState.h"
#include "Player/CSPlayerController.h"
#include "Player/CSPlayerState.h"
#include "Character/CSCharacterPlayer.h"
#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Actor/CSCameraViewProxy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Subsystem/CSPlayerSlotSubsystem.h"
#include "ChronoSpace.h"

ACSGameMode::ACSGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

    // Set our custom GameState
    GameStateClass = ACSGameState::StaticClass();
}

void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoEnableSplitScreen)
    {
        if (UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>())
        {
            CSSplitSubsystem->EnableSplitScreen();
        }
    }
}

void ACSGameMode::PostLogin(APlayerController* NewPlayer)
{
    ConnectedPlayers.AddUnique(NewPlayer);

    // Resolve the slot identity-first (UCSPlayerSlotSubsystem keys by UniqueNetId
    // and survives non-seamless ServerTravel), then stamp it onto the PlayerState
    // BEFORE Super::PostLogin — Super invokes RestartPlayer ->
    // GetDefaultPawnClassForController, which needs the slot at that moment.
    // If PS is null at this point (rare race on EOS reconnects), the fallback
    // in GetDefaultPawnClassForController will ask the subsystem directly.
    if (UCSPlayerSlotSubsystem* SlotSub = GetGameInstance()->GetSubsystem<UCSPlayerSlotSubsystem>())
    {
        const ECSPlayerSlot AssignedSlot = SlotSub->EnsureSlotForController(NewPlayer);
        if (ACSPlayerState* CSPS = NewPlayer->GetPlayerState<ACSPlayerState>())
        {
            CSPS->SetPlayerSlot(AssignedSlot);
        }
        else
        {
            UE_LOG(LogCS, Warning,
                TEXT("ACSGameMode::PostLogin: PlayerState null for %s; pawn class will fall back to subsystem lookup"),
                *NewPlayer->GetName());
        }
    }

    Super::PostLogin(NewPlayer);

    if (NewPlayer && NewPlayer->GetPawn())
    {
        ACSGameState* CSGameState = GetCSGameState();
        if (CSGameState)
        {
            CSGameState->AddPlayerToDeathTracking(NewPlayer->GetPawn());
        }

        UE_LOG(LogCS, Log, TEXT("Player logged in: %s"), *NewPlayer->GetName());

        OnPlayerLogin.Broadcast();
    }

    CreateProxiesForPlayer(NewPlayer);
    TrySplitScreenSetup();
}

void ACSGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    APlayerController* NewPlayer = Cast<APlayerController>(C);
    if (!NewPlayer)
        return;

    UE_LOG(LogCS, Log, TEXT("HandleSeamlessTravelPlayer: %s"), *NewPlayer->GetName());

    if (NewPlayer && NewPlayer->GetPawn())
    {
        ACSGameState* CSGameState = GetCSGameState();
        if (CSGameState)
        {
            CSGameState->AddPlayerToDeathTracking(NewPlayer->GetPawn());
        }

        UE_LOG(LogCS, Log, TEXT("Player rejoined (seamless): %s"), *NewPlayer->GetName());
        OnPlayerLogin.Broadcast();
    }

    ConnectedPlayers.AddUnique(NewPlayer);
    CreateProxiesForPlayer(NewPlayer);
    TrySplitScreenSetup();
}


void ACSGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        if (APawn* ExitingPawn = PC->GetPawn())
        {
            ACSGameState* CSGameState = GetCSGameState();
            if (CSGameState)
            {
                CSGameState->RemovePlayerFromDeathTracking(ExitingPawn);
            }

            UE_LOG(LogCS, Log, TEXT("Player logged out: %s"), *PC->GetName());
        }

        CleanupSplitScreenForPlayer(PC);
        ConnectedPlayers.Remove(PC);

        // Only release the slot for real disconnects, not for the transient
        // Logout that happens while the world is being torn down for a
        // ServerTravel — otherwise the player would lose their slot mid-travel
        // and re-receive whatever the lowest-free is on the new map.
        const bool bTravelling = GetWorld() && GetWorld()->bIsTearingDown;
        if (!bTravelling)
        {
            if (UCSPlayerSlotSubsystem* SlotSub = GetGameInstance()->GetSubsystem<UCSPlayerSlotSubsystem>())
            {
                SlotSub->ReleaseSlotForController(PC);
            }
        }
    }

    Super::Logout(Exiting);
}

UClass* ACSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (!InController)
    {
        return Super::GetDefaultPawnClassForController_Implementation(InController);
    }

    // Single source of truth for "which character": ECSPlayerSlot.
    // Primary: PlayerState (set in PostLogin from the slot subsystem).
    // Fallback: ask the subsystem directly, in case PlayerState hasn't attached
    //          yet on this controller (EOS reconnect race during RestartPlayer).
    ECSPlayerSlot Slot = ECSPlayerSlot::Player0;
    bool bSlotResolved = false;

    if (const ACSPlayerState* CSPS = InController->GetPlayerState<ACSPlayerState>())
    {
        Slot = CSPS->GetPlayerSlot();
        bSlotResolved = true;
    }
    else if (APlayerController* PC = Cast<APlayerController>(InController))
    {
        if (UCSPlayerSlotSubsystem* SlotSub = GetGameInstance()->GetSubsystem<UCSPlayerSlotSubsystem>())
        {
            Slot = SlotSub->EnsureSlotForController(PC);
            bSlotResolved = true;
            UE_LOG(LogCS, Warning,
                TEXT("GetDefaultPawnClassForController: PlayerState missing on %s, resolved via subsystem -> %s"),
                *PC->GetName(),
                Slot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"));
        }
    }

    if (bSlotResolved)
    {
        switch (Slot)
        {
        case ECSPlayerSlot::Player0:
            if (PawnClassPlayer0) return PawnClassPlayer0;
            break;
        case ECSPlayerSlot::Player1:
            if (PawnClassPlayer1) return PawnClassPlayer1;
            break;
        }
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool ACSGameMode::RespawnSinglePlayer(APawn* Player)
{
    if (!HasAuthority() || !IsValid(Player))
        return false;

    AController* Controller = Player->GetController();
    if (!IsValid(Controller))
    {
        UE_LOG(LogCS, Warning, TEXT("RespawnSinglePlayer: Controller is null"));
        return false;
    }

    ACSPlayerState* PS = Player->GetPlayerState<ACSPlayerState>();
    if (!IsValid(PS))
    {
        UE_LOG(LogCS, Warning, TEXT("RespawnSinglePlayer: No PlayerState"));
        return false;
    }

    ACSRespawnPoint* RespawnPoint = PS->GetPersonalRespawnPoint();
    if (!IsValid(RespawnPoint))
    {
        UE_LOG(LogCS, Warning, TEXT("RespawnSinglePlayer: No personal respawn point"));
        return false;
    }

    FTransform SpawnTM = RespawnPoint->GetActorTransform();
    SpawnTM.AddToTranslation(FVector(0.f, 0.f, 120.f));

    APawn* OldPawn = Player;

    Controller->UnPossess();

    if (IsValid(OldPawn))
    {
        OldPawn->DetachFromControllerPendingDestroy();
        OldPawn->SetActorEnableCollision(false);
        OldPawn->SetReplicateMovement(false);
        OldPawn->Destroy();
    }

    RestartPlayerAtTransform(Controller, SpawnTM);

    APawn* NewPawn = Controller->GetPawn();
    if (!IsValid(NewPawn))
    {
        UE_LOG(LogCS, Warning, TEXT("RespawnSinglePlayer: NewPawn is null"));
        return false;
    }

    const FVector SpawnLoc = SpawnTM.GetLocation();
    const FRotator SpawnRot = SpawnTM.GetRotation().Rotator();

    NewPawn->SetActorLocationAndRotation(
        SpawnLoc,
        SpawnRot,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    NewPawn->ForceNetUpdate();
    NewPawn->FlushNetDormancy();

    if (ACharacter* NewCharacter = Cast<ACharacter>(NewPawn))
    {
        if (UCharacterMovementComponent* MovementComp = NewCharacter->GetCharacterMovement())
        {
            MovementComp->StopMovementImmediately();
            MovementComp->Velocity = FVector::ZeroVector;
            MovementComp->SetMovementMode(MOVE_Walking);
            MovementComp->bForceNextFloorCheck = true;
            MovementComp->UpdateComponentVelocity();
        }
    }

    if (ACSPlayerController* CSPC = Cast<ACSPlayerController>(Controller))
    {
        CSPC->Client_ApplyRespawnView(SpawnRot);
    }

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->SetControlRotation(SpawnRot);
        PC->ClientSetRotation(SpawnRot, true);
    }

    if (ACSGameState* GS = GetCSGameState())
    {
        GS->HandlePlayerRevive(NewPawn);
    }

    UE_LOG(LogCS, Log, TEXT("RespawnSinglePlayer: %s -> %s"),
        *GetNameSafe(OldPawn),
        *GetNameSafe(NewPawn));

    return true;
}

ACSGameState* ACSGameMode::GetCSGameState() const
{
    return Cast<ACSGameState>(GameState);
}

void ACSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    UE_LOG(LogCS, Log, TEXT("CSGameMode::EndPlay"));
}

// ============================================================
// CameraViewProxy 생성 / 정리 — 분할 화면용 카메라 동기화 채널
// ============================================================

void ACSGameMode::CreateProxiesForPlayer(APlayerController* NewPlayer)
{
    if (!NewPlayer) return;

    // 1) 원격 클라이언트별 개별 Proxy 생성 — 클라가 자기 카메라를 RPC 로 올려, 다른 모두에게 리플리케이트
    if (!NewPlayer->IsLocalController())
    {
        UE_LOG(LogCS, Log, TEXT("CreateProxiesForPlayer: Creating client proxy for %s"), *NewPlayer->GetName());

        FActorSpawnParameters ClientProxyParams;
        ClientProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ClientProxyParams.Owner = NewPlayer;

        ACSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ACSCameraViewProxy>(
            ACSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ClientProxyParams
        );

        if (ClientProxy)
        {
            ClientProxy->SetReplicates(true);
            ClientProxy->SetReplicateMovement(false);
            ClientProxy->SetIsServerProxy(false);
            ClientCamProxies.Add(NewPlayer, ClientProxy);
        }
    }

    // 2) 서버 (ListenServer) 의 LocalPlayer 카메라용 Proxy — 한 번만 생성. 호스트가 자기 카메라를 채워 리플리케이트
    if (NewPlayer->IsLocalController() && !ServerCamProxy)
    {
        FActorSpawnParameters ServerProxyParams;
        ServerProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ServerCamProxy = GetWorld()->SpawnActor<ACSCameraViewProxy>(
            ACSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ServerProxyParams
        );

        if (ServerCamProxy)
        {
            ServerCamProxy->SetReplicates(true);
            ServerCamProxy->SetReplicateMovement(false);
            ServerCamProxy->SetIsServerProxy(true);
            UE_LOG(LogCS, Log, TEXT("Created ServerCamProxy (ListenServer POV)"));
        }
    }
}

void ACSGameMode::TrySplitScreenSetup()
{
    if (!bAutoEnableSplitScreen) return;
    if (GetWorld()->GetNetMode() != NM_ListenServer) return;
    if (ConnectedPlayers.Num() < 2) return;

    // 새 아키텍처: GameMode 는 더 이상 더미 LocalPlayer / SpectatorPawn 을 만들지 않는다.
    // CameraViewProxy 가 양쪽 카메라를 리플리케이트하고, UCSSplitScreenSubsystem 이
    // 매 프레임 UCSViewFamilyViewportClient 에 보조 뷰 카메라를 푸시한다.
    if (UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>())
    {
        CSSplitSubsystem->EnableSplitScreen();
    }
    UE_LOG(LogCS, Log, TEXT("TrySplitScreenSetup: ViewFamily-based split enabled"));
}

void ACSGameMode::CleanupSplitScreenForPlayer(APlayerController* ExitingPlayer)
{
    if (!ExitingPlayer) return;

    TObjectPtr<ACSCameraViewProxy>* FoundProxy = ClientCamProxies.Find(ExitingPlayer);
    if (FoundProxy && *FoundProxy)
    {
        (*FoundProxy)->Destroy();
        UE_LOG(LogCS, Log, TEXT("CleanupSplitScreenForPlayer: Destroyed client proxy for %s"), *ExitingPlayer->GetName());
    }
    ClientCamProxies.Remove(ExitingPlayer);
}
