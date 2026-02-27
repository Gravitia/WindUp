// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/CSGameMode.h"
#include "Game/CSGameState.h"
#include "Player/CSPlayerController.h"
#include "Player/CSPlayerState.h"
#include "Character/CSCharacterPlayer.h"
#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Actor/CSCameraViewProxy.h"
#include "Pawn/CSSpectatorPawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "HAL/PlatformMisc.h" // FPlatformUserId 사용을 위해 추가
#include "TimerManager.h" // GetWorldTimerManager() 사용을 위해
#include "Components/CapsuleComponent.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "ChronoSpace.h"

ACSGameMode::ACSGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

    // Set our custom GameState
    GameStateClass = ACSGameState::StaticClass();

    DummySpectatorPawnClass = ACSSpectatorPawn::StaticClass();
}

void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoEnableSplitScreen)
    {
        UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>();

        if ( CSSplitSubsystem )
        {
            CSSplitSubsystem->EnableSplitScreen();
        }
    }
}

void ACSGameMode::PostLogin(APlayerController* NewPlayer)
{
    ConnectedPlayers.AddUnique(NewPlayer);

    if (ACSPlayerState* CSPS = NewPlayer->GetPlayerState<ACSPlayerState>())
    {
        const int32 PlayerIndex = ConnectedPlayers.IndexOfByKey(NewPlayer);

        if (PlayerIndex == 0)
        {
            CSPS->SetPlayerSlot(ECSPlayerSlot::Player0);
        }
        else
        {
            CSPS->SetPlayerSlot(ECSPlayerSlot::Player1);
        }
    }

    // Super 
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

    

    // === 분리된 Proxy 시스템 ===

    // 1) 클라이언트별 개별 Proxy 생성 (모든 원격 클라이언트용)
    if (!NewPlayer->IsLocalController()) // 원격 클라이언트
    {
        UE_LOG(LogCS, Log, TEXT("ACSGameMode::PostLogin - %s"), *NewPlayer->GetName());
        FActorSpawnParameters ClientProxyParams;
        ClientProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ClientProxyParams.Owner = NewPlayer; // 클라이언트를 Owner로 설정

        ACSCameraViewProxy* ClientProxy = GetWorld()->SpawnActor<ACSCameraViewProxy>(
            ACSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ClientProxyParams
        );

        if (ClientProxy)
        {
            // 중요: 클라이언트에도 복제되도록 설정
            ClientProxy->SetReplicates(true);
            ClientProxy->SetReplicateMovement(false); // 카메라 데이터만 복제
            ClientProxy->SetIsServerProxy(false);

            // 클라이언트별 Proxy 맵에 추가
            ClientCamProxies.Add(NewPlayer, ClientProxy);
        }
    }

    // 2) 서버 로컬 플레이어용 Proxy 생성 (한 번만)
    if (NewPlayer->IsLocalController() && !ServerCamProxy)
    {
        FActorSpawnParameters ServerProxyParams;
        ServerProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Owner를 설정하지 않음 - 서버 전용 Proxy

        ServerCamProxy = GetWorld()->SpawnActor<ACSCameraViewProxy>(
            ACSCameraViewProxy::StaticClass(),
            FTransform::Identity,
            ServerProxyParams
        );

        if (ServerCamProxy)
        {
            // 서버 Proxy도 복제되도록 설정
            ServerCamProxy->SetReplicates(true);
            ServerCamProxy->SetReplicateMovement(false);
            ServerCamProxy->SetIsServerProxy(true);

            UE_LOG(LogCS, Warning, TEXT("CS Created ServerCamProxy (ListenServer POV, No Owner)"));
        }
    }

    if (bAutoEnableSplitScreen)
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer)
        {
            // 정확히 2명일 때만 실행 (중복 방지)
            if (ConnectedPlayers.Num() == 2 && !DummyPlayerController)
            {
                UE_LOG(LogCS, Warning, TEXT("SS Starting split screen setup..."));
                SetupOnlineSplitScreen();
            }
        }
    }
}

void ACSGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    APlayerController* NewPlayer = Cast<APlayerController>(C);
    if (!NewPlayer)
        return;

    UE_LOG(LogCS, Warning, TEXT("HandleSeamlessTravelPlayer: %s"), *NewPlayer->GetName());

    // === 기존 PostLogin 로직 일부 그대로 적용 ===
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

    // === Proxy 시스템 재생성 ===
    if (!NewPlayer->IsLocalController()) // 원격 클라용 Proxy
    {
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

    // 서버용 Proxy
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
            UE_LOG(LogCS, Warning, TEXT("SeamlessTravel: Created ServerCamProxy"));
        }
    }

    // === SplitScreen 재세팅 ===
    if (bAutoEnableSplitScreen)
    {
        if (GetWorld()->GetNetMode() == NM_ListenServer)
        {
            if (ConnectedPlayers.Num() == 2 && !DummyPlayerController)
            {
                UE_LOG(LogCS, Warning, TEXT("SeamlessTravel: Rebuilding split screen setup..."));
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
    }

    APlayerController* PC = Cast<APlayerController>(Exiting);
    if (PC)
    {
        ConnectedPlayers.Remove(PC);
    }

    Super::Logout(Exiting);
}

UClass* ACSGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (!InController)
    {
        return Super::GetDefaultPawnClassForController_Implementation(InController);
    }

    // 로컬 스플릿스크린은 기존 로직 유지 가능
    if (const APlayerController* PC = Cast<APlayerController>(InController))
    {
        if (const ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            const int32 Id = LP->GetControllerId();
            if (Id == 0 && PawnClassPlayer0) return PawnClassPlayer0;
            if (Id == 1 && PawnClassPlayer1) return PawnClassPlayer1;
        }
    }

    // 온라인/리스폰은 PlayerState 슬롯 기준
    if (const ACSPlayerState* CSPS = InController->GetPlayerState<ACSPlayerState>())
    {
        switch (CSPS->GetPlayerSlot())
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

    // 기존 Pawn 정리
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

void ACSGameMode::CreateDummyLocalPlayer()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;

    // 현재 로컬 플레이어 수 확인
    int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();

    if (CurrentLocalPlayers >= 2)
    {
        UE_LOG(LogCS, Warning, TEXT("SS Already have 2+ local players"));
        // return;
    }

    // 더미 로컬 플레이어 생성
    FPlatformUserId DummyUserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(1);
    FString OutError;
    ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

    if (!DummyLocalPlayer)
    {
        UE_LOG(LogCS, Error, TEXT("SS Failed to create dummy local player"));
        return;
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("SS Success to create dummy local player"));
    }

    // 더미 스펙테이터 폰 생성
    FVector SpawnLocation = FVector(0, 0, 0);
    FRotator SpawnRotation = FRotator::ZeroRotator;

    DummySpectatorPawn = GetWorld()->SpawnActor<ACSSpectatorPawn>(
        DummySpectatorPawnClass,
        SpawnLocation,
        SpawnRotation
    );

    if (!DummySpectatorPawn)
    {
        DummySpectatorPawn->Rename(TEXT("ServerSpectatorPawn"));
    }


    if (!DummySpectatorPawn)
    {
        UE_LOG(LogCS, Error, TEXT("SS Failed to spawn dummy spectator pawn"));
        return;
    }

    // 더미 플레이어 컨트롤러 생성
    DummyPlayerController = GetWorld()->SpawnActor<ACSPlayerController>();
    if (DummyPlayerController)
    {
        // 더미로 표시
        DummyPlayerController->SetAsDummyController(true);
        DummyPlayerController->SetPawn(nullptr);
        DummyPlayerController->SetPlayer(DummyLocalPlayer);
        DummyPlayerController->Possess(DummySpectatorPawn);

        UE_LOG(LogCS, Warning, TEXT("SS Dummy Local Player Created Successfully"));
    }
}

void ACSGameMode::AttachDummySpectatorToClient(APlayerController* RemoteClient)
{
    if (!RemoteClient || !RemoteClient->GetPawn())
    {
        UE_LOG(LogCS, Warning, TEXT("SS Server: Remote client or pawn not valid"));
        return;
    }

    APawn* ClientPawn = RemoteClient->GetPawn();
    USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh)
    {
        UE_LOG(LogCS, Warning, TEXT("SS Server: Client pawn has no skeletal mesh"));
        return;
    }

    if (!DummySpectatorPawn)
    {
        // 더미 폰 스폰
        DummySpectatorPawn = GetWorld()->SpawnActor<ACSSpectatorPawn>(
            DummySpectatorPawnClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator
        );
    }

    if (DummySpectatorPawn)
    {
        // 클라 캐릭터 스켈레톤 소켓에 Attach
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        DummySpectatorPawn->AttachToComponent(Mesh, AttachRules, FName("camera_socket"));
        // "head" 대신 캐릭터 스켈레톤 소켓 이름 사용

        // Pawn은 보이지 않게 설정
        DummySpectatorPawn->SetActorHiddenInGame(true);
        DummySpectatorPawn->SetActorEnableCollision(false);

        UE_LOG(LogCS, Warning, TEXT("SS DummySpectator attached to %s's skeleton"),
            *ClientPawn->GetName());
    }
}

void ACSGameMode::SyncDummyRotationWithProxy()
{
    // 1. 원격 클라 찾기
    
    APlayerController* RemoteClient = nullptr;

    if (RemoteClient == nullptr)
    {
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
            UE_LOG(LogCS, Warning, TEXT("SS RemoteClient null"));
            return;
        }
    }

    // 2. 해당 클라의 Proxy 가져오기
    ACSCameraViewProxy* ClientProxy = ClientCamProxies[RemoteClient];
    if ( ClientProxy == nullptr ) return;

    FRepCamInfo& RemoteClientCam = ClientProxy->GetReplicatedCamera();

    if (!DummySpectatorPawn || !DummyPlayerController)
    {
        UE_LOG(LogCS, Warning, TEXT("CS DummySpectatorPawn or DummyPlayerController invalid"));
        return;
    }

    // 3. 위치 동기화 (캐릭터 크기만큼 오프셋 적용)
    APawn* ClientPawn = RemoteClient->GetPawn();
    FVector TargetLoc = RemoteClientCam.Location; // 기본값: 클라 카메라 위치 그대로

    if (ClientPawn)
    {
        // 3-1) root에 offset 적용한 camera_socket 필요. 
        if (USkeletalMeshComponent* Mesh = ClientPawn->FindComponentByClass<USkeletalMeshComponent>())
        {
            if (Mesh->DoesSocketExist(TEXT("camera_socket")))
            {
                TargetLoc = Mesh->GetSocketLocation(TEXT("camera_socket"));
                // Socket이 없으면 기본으로 캐릭터의 중앙인듯. 
            }
        }
        else
        {
            UE_LOG(LogCS, Warning, TEXT("CS no head socket "));
        }
    }

    FVector NewLoc = FMath::VInterpTo(
        DummySpectatorPawn->GetActorLocation(),
        TargetLoc,
        GetWorld()->GetDeltaSeconds(),
        30.f // 보간 속도
    );

    DummySpectatorPawn->SetActorLocation(NewLoc);

    // 4. 회전은 클라 입력값을 그대로 쓰거나 무시 (옵션)
    //    여기서는 클라 카메라 회전 그대로 반영
    
    FRotator TargetRot = RemoteClientCam.Rotation;
    FRotator CurrentRot = DummyPlayerController->GetControlRotation();

    FRotator NewRot = FMath::RInterpTo(
        CurrentRot,
        TargetRot,
        GetWorld()->GetDeltaSeconds(),
        20.f // 보간 속도
    );

    DummyPlayerController->SetControlRotation(NewRot);

    // UE_LOG(LogCS, Verbose, TEXT("CS Server: Synced dummy location=%s, rotation=%s"), *NewLoc.ToString(), *NewRot.ToString());
}

void ACSGameMode::SetupOnlineSplitScreen()
{
    CreateDummyLocalPlayer();
    // 원격 클라 찾기 → 더미 스펙테이터 붙이기
    APlayerController* RemoteClient = nullptr;
    for (APlayerController* PC : ConnectedPlayers)
    {
        if (PC && !PC->IsLocalController())
        {
            RemoteClient = PC;
            break;
        }
    }
    AttachDummySpectatorToClient(RemoteClient);

    // === 회전 동기화 타이머 시작 ===
    // WeakLambda로 바꾸기. 
    GetWorldTimerManager().SetTimer(
        RotationSyncTimerHandle,
        FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                if (!IsValid(this) || !IsValid(GetWorld()) || GetWorld()->bIsTearingDown)
                    return;

                SyncDummyRotationWithProxy();
            }),
        0.016f,   // 60FPS 주기
        true      // 반복
    );
}

void ACSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    // 회전 동기화 타이머 종료
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(RotationSyncTimerHandle);
    }

    UE_LOG(LogCS, Warning, TEXT("CSGameMode::EndPlay - cleared rotation sync timer"));
}