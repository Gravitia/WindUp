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
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Subsystem/CSPlayerSlotSubsystem.h"
#include "ActorComponent/CSCustomGravityDirComponent.h"
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

    // 월드 기본 중력 방향은 레벨 시작 시 1회만 리셋 (이전 레벨의 중력 스위치 상태가 ServerTravel 을 넘어오지 않도록).
    // 캐릭터 BeginPlay 마다 리셋하면 리스폰/분신 스폰 때마다 스위치 상태가 되돌아간다.
    UCSCustomGravityDirComponent::OrgGravityDirection = FVector(0.0f, 0.0f, -1.0f);

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

    // 슬롯을 먼저 정하고 PlayerState 에 박은 뒤에 Super::PostLogin 을 부른다.
    // Super 가 RestartPlayer -> GetDefaultPawnClassForController 를 타므로
    // 그 시점에 슬롯이 이미 있어야 한다.
    ResolvePlayerSlotForPlayer(NewPlayer);

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

ECSPlayerSlot ACSGameMode::ResolvePlayerSlotForPlayer(APlayerController* NewPlayer)
{
    UGameInstance* GI = GetGameInstance();
    UCSPlayerSlotSubsystem* SlotSub = GI ? GI->GetSubsystem<UCSPlayerSlotSubsystem>() : nullptr;
    if (!NewPlayer || !SlotSub)
    {
        return ECSPlayerSlot::Player0;
    }

    // UCSPlayerSlotSubsystem 은 GameInstance 소유라 non-seamless ServerTravel 을 넘어 살아남는다.
    ECSPlayerSlot Slot = SlotSub->EnsureSlotForController(NewPlayer);

    // 마지막 방어선: 같은 슬롯을 이미 들고 있는 다른 접속자가 있으면 반대쪽으로 돌린다.
    // 슬롯 키가 어떤 이유로든 겹쳐도 "1번 하나, 2번 하나" 라는 규칙만은 깨지지 않게 한다.
    // 여기가 울리면 키 생성이 잘못된 것이므로 Warning 을 남긴다 (조용히 덮지 않는다).
    if (IsSlotHeldByOtherPlayer(NewPlayer, Slot))
    {
        const ECSPlayerSlot Other =
            (Slot == ECSPlayerSlot::Player0) ? ECSPlayerSlot::Player1 : ECSPlayerSlot::Player0;

        UE_LOG(LogCS, Warning,
            TEXT("ACSGameMode: %s was assigned %s but another connected player already holds it; using %s instead"),
            *NewPlayer->GetName(),
            Slot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"),
            Other == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"));

        Slot = Other;
        SlotSub->AssignSlotForController(NewPlayer, Slot);
    }

    if (ACSPlayerState* CSPS = NewPlayer->GetPlayerState<ACSPlayerState>())
    {
        CSPS->SetPlayerSlot(Slot);
    }
    else
    {
        // PS 가 아직 안 붙은 드문 경우. GetDefaultPawnClassForController 의
        // 폴백이 서브시스템에 직접 물어본다.
        UE_LOG(LogCS, Warning,
            TEXT("ACSGameMode: PlayerState null for %s; pawn class will fall back to subsystem lookup"),
            *NewPlayer->GetName());
    }

    UE_LOG(LogCS, Log, TEXT("ACSGameMode: %s -> %s"),
        *NewPlayer->GetName(),
        Slot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"));

    return Slot;
}

bool ACSGameMode::IsSlotHeldByOtherPlayer(const APlayerController* NewPlayer, ECSPlayerSlot Slot) const
{
    for (const TObjectPtr<APlayerController>& Other : ConnectedPlayers)
    {
        if (!IsValid(Other) || Other == NewPlayer)
        {
            continue;
        }

        if (const ACSPlayerState* OtherPS = Other->GetPlayerState<ACSPlayerState>())
        {
            if (OtherPS->GetPlayerSlot() == Slot)
            {
                return true;
            }
        }
    }
    return false;
}

void ACSGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    APlayerController* NewPlayer = Cast<APlayerController>(C);
    if (!NewPlayer)
        return;

    UE_LOG(LogCS, Log, TEXT("HandleSeamlessTravelPlayer: %s"), *NewPlayer->GetName());

    // SeamlessTravel 에서는 Super 안에서 이미 폰이 스폰됐다. 슬롯은
    // ACSPlayerState::CopyProperties 가 옮겨 준 값이 정답이므로 여기서 다시 정하지 않는다.
    // 대신 그 값을 서브시스템에 되돌려 넣어, 이후 non-seamless 트래블까지 같은 값이 남게 한다.
    if (const ACSPlayerState* CSPS = NewPlayer->GetPlayerState<ACSPlayerState>())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UCSPlayerSlotSubsystem* SlotSub = GI->GetSubsystem<UCSPlayerSlotSubsystem>())
            {
                SlotSub->AssignSlotForController(NewPlayer, CSPS->GetPlayerSlot());
            }
        }
    }

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

TSubclassOf<APawn> ACSGameMode::GetPawnClassForSlot(ECSPlayerSlot Slot) const
{
    switch (Slot)
    {
    case ECSPlayerSlot::Player0: return PawnClassPlayer0;
    case ECSPlayerSlot::Player1: return PawnClassPlayer1;
    }
    return nullptr;
}

ECSPlayerSlot ACSGameMode::ResolveBodySlotForPawn(const UWorld* World, const APawn* InPawn, bool& bOutResolved)
{
    bOutResolved = false;

    if (World == nullptr || !IsValid(InPawn))
    {
        return ECSPlayerSlot::Player0;
    }

    // 인스턴스가 아니라 CDO 를 읽는다. 클라이언트에는 GameMode 인스턴스가 없기 때문이다.
    const AGameStateBase* GameState = World->GetGameState();
    const ACSGameMode* GameModeCDO = GameState ? GameState->GetDefaultGameMode<ACSGameMode>() : nullptr;
    if (GameModeCDO == nullptr)
    {
        // 접속 직후엔 정상적으로 null 이다. 실패로 돌려주고 *캐시하지 않는다*.
        return ECSPlayerSlot::Player0;
    }

    const TSubclassOf<APawn> Class0 = GameModeCDO->GetPawnClassForSlot(ECSPlayerSlot::Player0);
    const TSubclassOf<APawn> Class1 = GameModeCDO->GetPawnClassForSlot(ECSPlayerSlot::Player1);

    // 두 슬롯이 같은 클래스면 몸을 구분할 방법이 없다. 추측하지 않고 실패로 돌려준다.
    if (Class0 == nullptr || Class1 == nullptr || Class0 == Class1)
    {
        return ECSPlayerSlot::Player0;
    }

    // 정확 비교다. IsA 를 쓰면 안 되는 이유는 선언부 주석 참고.
    UClass* const PawnClass = InPawn->GetClass();

    if (PawnClass == Class0)
    {
        bOutResolved = true;
        return ECSPlayerSlot::Player0;
    }

    if (PawnClass == Class1)
    {
        bOutResolved = true;
        return ECSPlayerSlot::Player1;
    }

    return ECSPlayerSlot::Player0;
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

    // 레벨 전환 등으로 아직 비어 있으면 여기서 한 번 더 채운다.
    // 사망 시점이면 서브레벨까지 다 올라와 있으니 OnPossess 때보다 성공률이 높다.
    ACSRespawnPoint::EnsureRespawnPoint(Player);

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
        // 사망 추적 키가 Pawn 이라 교체된 Pawn 으로 옮긴 뒤 부활 처리
        GS->TransferDeathTracking(OldPawn, NewPawn);
        GS->HandlePlayerRevive(NewPawn);
    }

    // 부활 연출은 새 Pawn 위치에서 모든 머신에
    if (ACSCharacterPlayer* NewCharacter = Cast<ACSCharacterPlayer>(NewPawn))
    {
        NewCharacter->Multicast_PlayReviveEffects();
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
            // 소스를 명시한다. 이게 없으면 ACSCameraViewProxy::Tick 이
            // UGameplayStatics::GetPlayerController(World, 0) 로 호스트를 *유추* 한다 —
            // 리슨 서버에서 결과는 맞지만 순서에 기대는 암묵적 의존이다.
            // 이 블록은 NewPlayer->IsLocalController() 안이라 NewPlayer 가 곧 호스트 PC 다.
            ServerCamProxy->SetSourcePC(NewPlayer);
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
