// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/CSGameMode.h"
#include "Game/CSGameState.h"
#include "Player/CSPlayerController.h"
#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Actor/CSCameraViewProxy.h"
#include "Pawn/CSSpectatorPawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ChronoSpace.h"

ACSGameMode::ACSGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

    // Set our custom GameState
    GameStateClass = ACSGameState::StaticClass();

    CurrentRespawnPoint = nullptr;

    DummySpectatorPawnClass = ACSSpectatorPawn::StaticClass();
}

void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoEnableSplitScreen)
    {
        UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>();
        if (CSSplitSubsystem)
        {
            CSSplitSubsystem->EnableSplitScreen();
        }
    }
}

void ACSGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (NewPlayer && NewPlayer->GetPawn())
    {
        // Add player to GameState death tracking
        ACSGameState* CSGameState = GetCSGameState();
        if (CSGameState)
        {
            CSGameState->AddPlayerToDeathTracking(NewPlayer->GetPawn());
        }

        UE_LOG(LogCS, Log, TEXT("Player logged in: %s"), *NewPlayer->GetName());
        OnPlayerLogin.Broadcast();
    }

    ConnectedPlayers.AddUnique(NewPlayer);

    // === 카메라 프록시 시스템 (유지) ===
    // 프록시는 카메라 정보 복제용으로 유지하되, 더미 플레이어는 SetViewTarget 사용

    // 1) 클라이언트별 개별 Proxy 생성 (모든 원격 클라이언트용)
    if (!NewPlayer->IsLocalController()) // 원격 클라이언트
    {
        UE_LOG(LogCS, Log, TEXT("Creating camera proxy for remote client: %s"), *NewPlayer->GetName());

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

    // 2) 서버 로컬 플레이어용 Proxy 생성 (한 번만)
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
            UE_LOG(LogCS, Log, TEXT("Created ServerCamProxy for local player"));
        }
    }

    // 스플릿 스크린 자동 설정
    if (bAutoEnableSplitScreen)
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer)
        {
            // 정확히 2명일 때만 실행 (중복 방지)
            if (ConnectedPlayers.Num() == 2 && !DummyPlayerController)
            {
                UE_LOG(LogCS, Log, TEXT("Starting split screen setup with SetViewTarget"));
                SetupOnlineSplitScreen();
            }
        }
    }
}

void ACSGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        if (APawn* ExitingPawn = PC->GetPawn())
        {
            // Remove player from GameState death tracking
            ACSGameState* CSGameState = GetCSGameState();
            if (CSGameState)
            {
                CSGameState->RemovePlayerFromDeathTracking(ExitingPawn);
            }
            UE_LOG(LogCS, Log, TEXT("Player logged out"));
        }

        // 연결된 플레이어 목록에서 제거
        ConnectedPlayers.Remove(PC);

        // 카메라 프록시 정리
        if (ClientCamProxies.Contains(PC))
        {
            if (ACSCameraViewProxy* Proxy = ClientCamProxies[PC])
            {
                Proxy->Destroy();
            }
            ClientCamProxies.Remove(PC);
        }

        // 더미 플레이어가 이 플레이어를 보고 있었다면 ViewTarget 해제
        if (DummyPlayerController && PC->GetPawn() &&
            DummyPlayerController->GetViewTarget() == PC->GetPawn())
        {
            DummyPlayerController->SetViewTarget(nullptr);
            UE_LOG(LogCS, Warning, TEXT("ViewTarget cleared due to player logout"));
        }
    }

    Super::Logout(Exiting);
}

UClass* ACSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (const APlayerController* PC = Cast<APlayerController>(InController))
    {
        // 1) 로컬 스플릿스크린: ControllerId 기준(0,1,2…)
        if (const ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            const int32 Id = LP->GetControllerId();
            if (Id == 0 && PawnClassPlayer0) return PawnClassPlayer0;
            if (Id == 1 && PawnClassPlayer1) return PawnClassPlayer1;
        }
    }

    // 2) 온라인 멀티플레이어: 현재 플레이어 수 기준
    if (HasAuthority())
    {
        int32 CurrentPlayerCount = GetNumPlayers();

        if (CurrentPlayerCount == 1 && PawnClassPlayer0)
        {
            UE_LOG(LogCS, Log, TEXT("Using PawnClassPlayer0 for first online player"));
            return PawnClassPlayer0;
        }
        else if (CurrentPlayerCount == 2 && PawnClassPlayer1)
        {
            UE_LOG(LogCS, Log, TEXT("Using PawnClassPlayer1 for second online player"));
            return PawnClassPlayer1;
        }
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ACSGameMode::SetupOnlineSplitScreen()
{
    UE_LOG(LogCS, Log, TEXT("CSGameMode::SetupOnlineSplitScreen called - Using SetViewTarget"));

    // 1. 더미 로컬 플레이어 생성
    CreateDummyLocalPlayer();

    // 2. 원격 클라이언트 찾기
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogCS, Error, TEXT("Remote client or pawn not valid for SetViewTarget"));
        return;
    }

    if (!DummyPlayerController)
    {
        UE_LOG(LogCS, Error, TEXT("DummyPlayerController not created"));
        return;
    }

    // === 3. SetViewTarget 설정 ===
    APawn* RemoteClientPawn = RemoteClient->GetPawn();

    // 즉시 ViewTarget 설정
    DummyPlayerController->SetViewTarget(RemoteClientPawn);

    // 부드러운 전환을 원한다면 아래 사용
    // DummyPlayerController->SetViewTargetWithBlend(RemoteClientPawn, 0.0f, EViewTargetBlendFunction::VTBlend_Linear);

    UE_LOG(LogCS, Log, TEXT("SetViewTarget: DummyController now viewing %s's pawn directly"),
        *RemoteClient->GetName());

    // 4. 카메라 컴포넌트 확인 (정보용)
    if (UCameraComponent* CameraComp = RemoteClientPawn->FindComponentByClass<UCameraComponent>())
    {
        UE_LOG(LogCS, Log, TEXT("Remote pawn has camera component - using it for view"));
    }
    else if (USpringArmComponent* SpringArm = RemoteClientPawn->FindComponentByClass<USpringArmComponent>())
    {
        UE_LOG(LogCS, Log, TEXT("Remote pawn has spring arm - using it for camera offset"));
    }
    else
    {
        UE_LOG(LogCS, Log, TEXT("Remote pawn has no camera components - using pawn location"));
    }

    // 5. 선택사항: ViewTarget 유효성 체크 타이머
    if (bUseViewTargetValidation)
    {
        GetWorldTimerManager().SetTimer(
            ViewTargetValidationTimer,
            this,
            &ACSGameMode::ValidateAndUpdateViewTarget,
            0.5f,  // 0.5초마다 체크
            true
        );
    }

    // 6. 선택사항: 추가 카메라 동기화가 필요한 경우
    if (bUseCameraProxySync)
    {
        GetWorldTimerManager().SetTimer(
            CameraProxySyncTimer,
            this,
            &ACSGameMode::SyncDummyCameraWithProxy,
            0.016f,  // 60fps
            true
        );
    }

    UE_LOG(LogCS, Log, TEXT("Split screen setup completed with SetViewTarget"));
}

void ACSGameMode::CreateDummyLocalPlayer()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogCS, Error, TEXT("GameInstance is null"));
        return;
    }

    // 현재 로컬 플레이어 수 확인
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogCS, Warning, TEXT("Already have %d local players"), CurrentLocalPlayers);
        // 필요시 return 추가
    }

    // 더미 로컬 플레이어 생성
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogCS, Error, TEXT("Failed to create dummy local player: %s"), *OutError);
        return;
    }

    UE_LOG(LogCS, Log, TEXT("Successfully created dummy local player"));

    // 더미 플레이어 컨트롤러 생성
    DummyPlayerController = GetWorld()->SpawnActor<ACSPlayerController>();
    if (!DummyPlayerController)
    {
        UE_LOG(LogCS, Error, TEXT("Failed to spawn dummy player controller"));
        return;
    }

    // 더미 컨트롤러 설정
    DummyPlayerController->SetAsDummyController(true);
    DummyPlayerController->SetPlayer(DummyLocalPlayer);

    // 더미 스펙테이터 폰 생성 (선택사항 - SetViewTarget에서는 필수 아님)
    if (bCreateDummySpectatorPawn && DummySpectatorPawnClass)
    {
        FVector SpawnLocation = FVector::ZeroVector;
        FRotator SpawnRotation = FRotator::ZeroRotator;

        DummySpectatorPawn = GetWorld()->SpawnActor<ACSSpectatorPawn>(
            DummySpectatorPawnClass,
            SpawnLocation,
            SpawnRotation
        );

        if (DummySpectatorPawn)
        {
            DummySpectatorPawn->SetActorHiddenInGame(true);
            DummySpectatorPawn->SetActorEnableCollision(false);
            DummyPlayerController->Possess(DummySpectatorPawn);
            UE_LOG(LogCS, Log, TEXT("Dummy spectator pawn created and possessed"));
        }
    }

    UE_LOG(LogCS, Log, TEXT("Dummy local player setup completed"));
}

void ACSGameMode::ValidateAndUpdateViewTarget()
{
    if (!DummyPlayerController)
    {
        return;
    }

    // 원격 클라이언트 찾기
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogCS, Warning, TEXT("Remote client lost - clearing view target"));
        DummyPlayerController->SetViewTarget(nullptr);
        return;
    }

    // ViewTarget이 올바른지 확인하고 필요시 재설정
    APawn* RemoteClientPawn = RemoteClient->GetPawn();
    if (DummyPlayerController->GetViewTarget() != RemoteClientPawn)
    {
        DummyPlayerController->SetViewTarget(RemoteClientPawn);
        UE_LOG(LogCS, Log, TEXT("ViewTarget updated to %s's pawn"), *RemoteClient->GetName());
    }
}

void ACSGameMode::SyncDummyCameraWithProxy()
{
    if (!DummyPlayerController)
    {
        return;
    }

    // 원격 클라이언트의 프록시에서 추가 카메라 정보 가져오기
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }

    if (!RemoteClient)
    {
        return;
    }
    /*
    // 프록시에서 카메라 정보 가져오기 (필요시)
    ACSCameraViewProxy** FoundProxy = ClientCamProxies.Find(RemoteClient);
    if (FoundProxy && *FoundProxy)
    {
        const FRepCamInfo& CamInfo = (*FoundProxy)->GetReplicatedCamera();

        // 추가적인 카메라 조정이 필요한 경우 여기서 처리
        // 예: FOV 조정, 특별한 카메라 효과 등

        UE_LOG(LogCS, Verbose, TEXT("Camera proxy sync - Location: %s, Rotation: %s"),
            *CamInfo.Location.ToString(), *CamInfo.Rotation.ToString());
    }
    */
}

// === Deprecated Functions (하위 호환성) ===

void ACSGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
    // SetViewTarget 방식을 사용하므로 더 이상 필요 없음
    UE_LOG(LogCS, Warning, TEXT("AttachDummySpectatorToClient is deprecated - using SetViewTarget instead"));

    // 하위 호환성을 위해 SetViewTarget 호출
    if (RemoteClient && RemoteClient->GetPawn() && DummyPlayerController)
    {
        DummyPlayerController->SetViewTarget(RemoteClient->GetPawn());
    }
}

void ACSGameMode::SyncDummyRotationWithProxy()
{
    // SetViewTarget이 자동으로 처리하므로 더 이상 필요 없음
    UE_LOG(LogCS, Warning, TEXT("SyncDummyRotationWithProxy is deprecated - SetViewTarget handles this automatically"));

    // 하위 호환성을 위해 ViewTarget 유효성만 체크
    ValidateAndUpdateViewTarget();
}

// === Respawn System Functions ===

void ACSGameMode::SetCurrentRespawnPoint(ACSRespawnPoint* NewRespawnPoint)
{
    if (CurrentRespawnPoint != NewRespawnPoint)
    {
        CurrentRespawnPoint = NewRespawnPoint;
        OnRespawnPointChanged(NewRespawnPoint);
        UE_LOG(LogCS, Log, TEXT("Current respawn point updated"));
    }
}

void ACSGameMode::RespawnAllPlayersAtCurrentPoint()
{
    if (!CurrentRespawnPoint)
    {
        UE_LOG(LogCS, Warning, TEXT("No current respawn point set"));
        return;
    }

    ACSGameState* CSGameState = GetCSGameState();
    if (!CSGameState)
    {
        UE_LOG(LogCS, Warning, TEXT("CSGameState not found for respawn"));
        return;
    }

    TArray<APawn*> AllPlayers;
    AllPlayers.Append(CSGameState->GetDeadPlayers());
    AllPlayers.Append(CSGameState->GetAlivePlayers());

    for (APawn* Player : AllPlayers)
    {
        if (Player && CurrentRespawnPoint)
        {
            CurrentRespawnPoint->SpawnPlayerHere(Player);
            CSGameState->HandlePlayerRevive(Player);
        }
    }

    OnAllPlayersRespawned();
    UE_LOG(LogCS, Log, TEXT("All players respawned at current respawn point"));
}

void ACSGameMode::HandlePlayerDeath(APawn* DeadPlayer)
{
    ACSGameState* CSGameState = GetCSGameState();
    if (CSGameState)
    {
        CSGameState->HandlePlayerDeath(DeadPlayer);
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("CSGameState not found for player death handling"));
    }
}

bool ACSGameMode::RespawnSinglePlayer(APawn* Player)
{
    if (!Player)
    {
        UE_LOG(LogCS, Warning, TEXT("Invalid player for respawn"));
        return false;
    }

    if (!CurrentRespawnPoint)
    {
        UE_LOG(LogCS, Warning, TEXT("No current respawn point set for single player respawn"));
        return false;
    }

    CurrentRespawnPoint->SpawnPlayerHere(Player);

    ACSGameState* CSGameState = GetCSGameState();
    if (CSGameState)
    {
        CSGameState->HandlePlayerRevive(Player);
    }

    UE_LOG(LogCS, Log, TEXT("Single player respawned: %s"), *Player->GetName());
    return true;
}

bool ACSGameMode::RespawnPlayerAtCurrentPoint(APawn* Player)
{
    return RespawnSinglePlayer(Player);
}

ACSGameState* ACSGameMode::GetCSGameState() const
{
    return Cast<ACSGameState>(GameState);
}