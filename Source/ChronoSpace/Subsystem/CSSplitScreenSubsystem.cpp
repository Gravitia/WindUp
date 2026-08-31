// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSSplitScreenSubsystem.h"

#include "UI/CSViewFamilyViewportClient.h"
#include "Actor/CSCameraViewProxy.h"
#include "Character/CSCharacterPlayer.h"
#include "Game/CSGameMode.h"
#include "Player/CSPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "ChronoSpace.h"

static TAutoConsoleVariable<int32> CVarCSSplitScreenDebug(
    TEXT("cs.SplitScreen.DebugDraw"),
    0,
    TEXT("0=off, 1=draw secondary camera position (yellow sphere) and forward (red line) every Tick"),
    ECVF_Cheat);

/**
 * 화면 절반을 *몸* 에 고정한다 — P1 몸 왼쪽, P2 몸 오른쪽.
 *
 * 프로세스 전역이라 PIE 두 창이 같은 값을 본다. GameInstance 별 멤버로 두면
 * 창마다 따로 켜야 해서 두 창의 배치가 어긋날 수 있었다.
 * 비셰이핑 기본 1 — 디버그 툴을 쓸 때 설정 없이 바로 적용되기를 원했기 때문이다.
 */
static TAutoConsoleVariable<int32> CVarCSSplitScreenFixedSide(
    TEXT("cs.SplitScreen.FixedSide"),
#if UE_BUILD_SHIPPING
    0,
#else
    1,
#endif
    TEXT("1=fix screen halves to the bodies (P1 left / P2 right). 0=each player sees own body on the right (shipping behaviour)."),
    ECVF_Cheat);

namespace CSSplitScreenLayout
{
    /**
     * 상대를 못 찾은 프레임에 직전 대상을 몇 프레임까지 유지할지.
     *
     * 캐릭터 스왑 직후 두 폰의 PlayerState 역포인터가 한두 프레임 어긋난다
     * (APawn::OnRep_PlayerState 도착 순서). 그동안 보조 뷰를 꺼 버리면 화면이 풀스크린으로
     * 한 번 번쩍인다. 유예는 그 구간만 메운다 — 유지 대상이 "지금 내가 모는 몸" 이면
     * 절대 쓰지 않으므로(아래 가드) 원래 버그가 이 경로로 되살아나지 않는다.
     */
    static constexpr int32 MaxSecondaryHoldFrames = 6;
}

UCSSplitScreenSubsystem::UCSSplitScreenSubsystem()
{
}

void UCSSplitScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bSplitScreenActive = false;
    CurrentAlpha = 0.f;
    TargetAlpha = 0.f;
}

void UCSSplitScreenSubsystem::Deinitialize()
{
    if (UCSViewFamilyViewportClient* VC = CachedViewportClient.Get())
    {
        VC->ClearSecondaryView();
    }
    CachedViewportClient.Reset();
    CachedRemoteProxy.Reset();
    LastGoodRemoteBody.Reset();
    SecondaryHoldFrames = 0;
    bLastMainViewOnRight = true;
    bBodySlotWarnLogged = false;
    Super::Deinitialize();
}

bool UCSSplitScreenSubsystem::ResolveViewportClient()
{
    if (CachedViewportClient.IsValid())
    {
        return true;
    }
    // PIE 멀티 윈도우 안전성: GEngine->GameViewport 는 전역 단일이므로 잘못된 PIE 의 VC 를 잡을 수 있음.
    // GameInstance 별 GetGameViewportClient() 를 사용해 *이 Subsystem 이 속한 PIE* 의 VC 를 정확히 찾는다.
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return false;
    }
    UGameViewportClient* GVC = GI->GetGameViewportClient();
    UCSViewFamilyViewportClient* VC = Cast<UCSViewFamilyViewportClient>(GVC);
    if (!VC)
    {
        return false;
    }
    CachedViewportClient = VC;
    return true;
}

ACSCameraViewProxy* UCSSplitScreenSubsystem::ResolveRemoteProxy() const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return nullptr;

    // 로컬에서 *상대 플레이어* 의 카메라를 들고 있는 Proxy 를 찾는다.
    //
    // - 호스트 (ListenServer): 원격 클라가 보낸 ClientProxy 를 사용 (Owner != 로컬 PC)
    // - 클라이언트: 호스트의 ServerProxy 를 사용 (bIsServerProxy == true)
    APlayerController* LocalPC = nullptr;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (PC->IsLocalController())
            {
                LocalPC = PC;
                break;
            }
        }
    }

    const ENetMode NetMode = World->GetNetMode();

    for (TActorIterator<ACSCameraViewProxy> It(World); It; ++It)
    {
        ACSCameraViewProxy* Proxy = *It;
        if (!Proxy) continue;

        if (NetMode == NM_Client)
        {
            // 클라: ServerProxy 를 보조 뷰로 사용 (호스트의 카메라)
            if (Proxy->IsServerProxy())
            {
                return Proxy;
            }
        }
        else if (NetMode == NM_ListenServer)
        {
            // 호스트: 원격 클라이언트가 소유한 Proxy 를 보조 뷰로 사용
            if (!Proxy->IsServerProxy())
            {
                APlayerController* OwnerPC = Cast<APlayerController>(Proxy->GetOwner());
                if (OwnerPC && OwnerPC != LocalPC)
                {
                    return Proxy;
                }
            }
        }
        else if (NetMode == NM_Standalone)
        {
            // 싱글: 보조 뷰 없음 (분할 화면 비활성)
            return nullptr;
        }
    }
    return nullptr;
}

bool UCSSplitScreenSubsystem::ResolveSplitViewPair(ACSCharacterPlayer*& OutLocalBody, ACSCharacterPlayer*& OutRemoteBody) const
{
    OutLocalBody = nullptr;
    OutRemoteBody = nullptr;

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return false;

    ACSPlayerController* LocalPC = ResolveLocalPlayerController();
    if (!LocalPC) return false;

    OutLocalBody = Cast<ACSCharacterPlayer>(LocalPC->GetPawn());

    const AGameStateBase* GameState = World->GetGameState();
    if (!GameState) return false;

    const APlayerState* LocalPS = LocalPC->PlayerState;

    // PlayerArray 는 모든 머신에 복제된다. 2인 세션이면 나 아닌 하나가 곧 상대다.
    // (자세한 근거는 선언부 주석 참고)
    int32 CandidateCount = 0;
    for (const TObjectPtr<APlayerState>& PlayerStatePtr : GameState->PlayerArray)
    {
        APlayerState* PS = PlayerStatePtr.Get();
        if (!IsValid(PS) || PS == LocalPS) continue;
        if (PS->IsABot() || PS->IsOnlyASpectator()) continue;

        ACSCharacterPlayer* Body = Cast<ACSCharacterPlayer>(PS->GetPawn());
        if (!IsValid(Body)) continue;

        ++CandidateCount;
        if (OutRemoteBody == nullptr)
        {
            OutRemoteBody = Body;
        }
    }

    if (CandidateCount > 1)
    {
        // 이 분할 화면은 2인 전용이다 (ECSPlayerSlot 이 둘뿐). 조용히 첫 번째를 쓰되 소리는 낸다.
        UE_LOG(LogCS, VeryVerbose, TEXT("SplitScreen: %d remote candidates, using the first"), CandidateCount);
    }

    return OutRemoteBody != nullptr;
}

ACSPlayerController* UCSSplitScreenSubsystem::ResolveLocalPlayerController() const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return nullptr;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ACSPlayerController* PC = Cast<ACSPlayerController>(It->Get());
        if (PC && PC->IsLocalController())
        {
            return PC;
        }
    }
    return nullptr;
}

bool UCSSplitScreenSubsystem::IsFixedSideLayoutActive() const
{
#if UE_BUILD_SHIPPING
    // 셰이핑에서는 런타임에 켤 방법이 없다. 컴파일 타임에 잘라낸다.
    return false;
#else
    return CVarCSSplitScreenFixedSide.GetValueOnGameThread() != 0;
#endif
}

void UCSSplitScreenSubsystem::SetDebugFixedSplitSide(bool bEnable)
{
    // 상태 소유자는 CVar 하나다. 서브시스템이나 PlayerController 에 사본을 두면
    // "레이아웃은 고정인데 체크박스는 비어 있는" 어긋남이 생긴다.
    CVarCSSplitScreenFixedSide->Set(bEnable ? 1 : 0, ECVF_SetByCode);
}

bool UCSSplitScreenSubsystem::IsDebugFixedSplitSide() const
{
    return IsFixedSideLayoutActive();
}

void UCSSplitScreenSubsystem::UpdateSplitSideLayout()
{
    UCSViewFamilyViewportClient* VC = CachedViewportClient.Get();
    if (!VC) return;

    // bSwapLeftRight 는 "메인 뷰(= 이 창이 조작 중인 몸)를 오른쪽에 둔다" 는 뜻이다.

    if (!IsFixedSideLayoutActive())
    {
        // 셰이핑 동작: 두 사람 모두 자기 몸을 오른쪽에서 본다.
        bLastMainViewOnRight = true;
        VC->SetSwapLeftRight(true);
        return;
    }

    // 고정 배치: 화면 절반을 *몸* 에 묶는다 — P1 몸 왼쪽, P2 몸 오른쪽.
    // 판정 기준이 "누가 조작하느냐" 가 아니라 "어느 몸이냐" 라서 캐릭터를 맞바꿔도 배치가 유지된다.
    // (예전엔 GetDrivenBodySlot() 을 썼는데, 그 값은 서버 OnPossess 에서만 갱신되는
    //  COND_OwnerOnly 복제라 클라 배치가 왕복 지연만큼 늦었다)
    ACSCharacterPlayer* LocalBody = nullptr;
    ACSCharacterPlayer* RemoteBody = nullptr;
    ResolveSplitViewPair(LocalBody, RemoteBody);

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

    bool bSlotResolved = false;
    const ECSPlayerSlot LocalBodySlot = ACSGameMode::ResolveBodySlotForPawn(World, LocalBody, bSlotResolved);

    if (bSlotResolved)
    {
        bLastMainViewOnRight = (LocalBodySlot == ECSPlayerSlot::Player1);
        bBodySlotWarnLogged = false;
    }
    else if (!bBodySlotWarnLogged)
    {
        // 판정에 실패하면 추측하지 않고 마지막 값을 유지한다.
        // 매 프레임 뒤집히면 두 뷰의 ViewRect 가 계속 자리를 바꿔 TSR/자동노출 히스토리가 리셋된다.
        bBodySlotWarnLogged = true;
        UE_LOG(LogCS, Warning,
            TEXT("SplitScreen: 몸의 슬롯을 판정하지 못해 좌우 배치를 유지한다 (pawn=%s). "
                 "GameState 가 아직 안 왔거나 BP_CSGameMode 의 PawnClassPlayer0/1 이 비었거나 서로 같다."),
            *GetNameSafe(LocalBody));
    }

    VC->SetSwapLeftRight(bLastMainViewOnRight);
}

void UCSSplitScreenSubsystem::PushSecondaryCamera()
{
    UCSViewFamilyViewportClient* VC = CachedViewportClient.Get();
    if (!VC) return;

    ACSCameraViewProxy* Proxy = CachedRemoteProxy.Get();
    if (!Proxy)
    {
        Proxy = ResolveRemoteProxy();
        if (Proxy)
        {
            CachedRemoteProxy = Proxy;
        }
    }

    if (!Proxy)
    {
        VC->ClearSecondaryView();
        bHasSmoothedSecondary = false;
        return;
    }

    // ── 보조 뷰 대상(anchor) 을 *매 프레임* 다시 정한다. 캐시하지 않는다. ──
    //
    // 예전엔 CachedRemoteCharacter 에 캐시해 두고 null 일 때만 재해석했다. 그런데 디버그
    // 캐릭터 스왑은 액터를 죽이지 않고 조작자만 바꾸므로, TWeakObjectPtr 는 계속 유효한 채
    // "이제는 틀린" 대상을 가리켰다. 그래서 스왑 후 메인 뷰와 보조 뷰가 같은 몸을 비췄다
    // (사용자 보고: "p2 일 때는 양쪽 화면이 다 p2 로 나온다").
    ACSCharacterPlayer* LocalBody = nullptr;
    ACSCharacterPlayer* PairRemoteBody = nullptr;
    ResolveSplitViewPair(LocalBody, PairRemoteBody);

    // 같은 몸이면 절대 보조 뷰를 그리지 않는다. 이 가드가 위 버그를 구조적으로 막는다.
    if (PairRemoteBody == LocalBody)
    {
        PairRemoteBody = nullptr;
    }

    ACSCharacterPlayer* RemoteChar = nullptr;
    if (IsValid(PairRemoteBody))
    {
        RemoteChar = PairRemoteBody;
        SecondaryHoldFrames = 0;
    }
    else
    {
        // 스왑 직후 한두 프레임의 공백만 메운다. 유지 대상이 지금 내가 모는 몸이면 쓰지 않는다.
        ACSCharacterPlayer* HeldBody = LastGoodRemoteBody.Get();
        if (IsValid(HeldBody) && HeldBody != LocalBody &&
            SecondaryHoldFrames < CSSplitScreenLayout::MaxSecondaryHoldFrames)
        {
            RemoteChar = HeldBody;
            ++SecondaryHoldFrames;
        }
    }

    if (!IsValid(RemoteChar))
    {
        LastGoodRemoteBody.Reset();
        SecondaryHoldFrames = 0;
        VC->ClearSecondaryView();
        bHasSmoothedSecondary = false;
        return;
    }

    // 대상이 바뀐 프레임에는 보간을 끊는다. 안 하면 카메라가 옛 몸에서 새 몸으로 미끄러진다
    // (두 몸이 SnapDistance 안쪽에 있으면 스냅 처리에도 안 걸린다).
    if (LastGoodRemoteBody.Get() != RemoteChar)
    {
        bHasSmoothedSecondary = false;
    }
    LastGoodRemoteBody = RemoteChar;

    const FRepCamInfo& TargetCam = Proxy->GetReplicatedCamera();

    // 유효성 체크 — 회전과 ArmLength 모두 0 이면 아직 미수신
    if (TargetCam.Rotation.IsNearlyZero() && TargetCam.ArmLength < KINDA_SMALL_NUMBER)
    {
        VC->ClearSecondaryView();
        bHasSmoothedSecondary = false;
        return;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    const float DT = (World ? World->GetDeltaSeconds() : 0.016f);

    // ===== anchor 계산 =====
    // 핵심: UE CharacterMovement NetSmoothing 은 Mesh 에만 적용되고 Capsule(RootComponent) 는
    // server snapshot 시각마다 step-up 으로 점프한다. SpringArm 이 RootComponent 에 attach 되어
    // 있으므로 GetComponentLocation() 도 step-up 신호.
    //
    // 해결: Mesh 의 World location 에서 Mesh.RelativeLocation 을 빼면 NetSmoothing 이 적용된
    // *부드러운 ActorLocation* 이 나온다. 그 위에 SpringArm.RelativeLocation 을 적용하면
    // 부드러운 SpringArm 부착점이 됨.
    USpringArmComponent* RemoteArm = RemoteChar->FindComponentByClass<USpringArmComponent>();
    USkeletalMeshComponent* RemoteMesh = RemoteChar->GetMesh();

    FVector RawAnchor;
    if (RemoteMesh && RemoteArm)
    {
        const FQuat ActorQuat = RemoteChar->GetActorQuat();
        const FVector SmoothActorLoc = RemoteMesh->GetComponentLocation()
            - ActorQuat.RotateVector(RemoteMesh->GetRelativeLocation());
        RawAnchor = SmoothActorLoc + ActorQuat.RotateVector(RemoteArm->GetRelativeLocation());
    }
    else if (RemoteArm)
    {
        RawAnchor = RemoteArm->GetComponentLocation();
    }
    else
    {
        RawAnchor = RemoteChar->GetActorLocation();
    }

    // ===== 보간 (Anchor / 회전 / FOV / ArmLength) =====
    const float TgtFOV = (TargetCam.FOV > KINDA_SMALL_NUMBER) ? TargetCam.FOV : 90.f;
    // ArmLength 가 0 이면 (구버전 클라이언트 등) 원격 캐릭터의 로컬 SpringArm 값으로 fallback
    const float TgtArm = (TargetCam.ArmLength > KINDA_SMALL_NUMBER)
        ? TargetCam.ArmLength
        : (RemoteArm ? RemoteArm->TargetArmLength : 0.f);

    // 첫 수신이거나 거리가 크면 즉시 스냅 (텔레포트/리스폰 대응)
    const float AnchorJump = FVector::Dist(SmoothedAnchorLocation, RawAnchor);
    if (!bHasSmoothedSecondary || AnchorJump > SnapDistance)
    {
        SmoothedAnchorLocation = RawAnchor;
        SmoothedSecondaryRotation = TargetCam.Rotation;
        SmoothedSecondaryFOV = TgtFOV;
        SmoothedArmLength = TgtArm;
        bHasSmoothedSecondary = true;
    }
    else
    {
        // Anchor 추가 평활 — NetSmoothing 이 disabled 인 환경에서도 떨림 흡수
        SmoothedAnchorLocation = FMath::VInterpTo(SmoothedAnchorLocation, RawAnchor, DT, AnchorInterpSpeed);
        SmoothedSecondaryRotation = FMath::RInterpTo(SmoothedSecondaryRotation, TargetCam.Rotation, DT, RotationInterpSpeed);
        SmoothedSecondaryFOV = FMath::FInterpTo(SmoothedSecondaryFOV, TgtFOV, DT, FOVInterpSpeed);
        SmoothedArmLength = FMath::FInterpTo(SmoothedArmLength, TgtArm, DT, ArmLengthInterpSpeed);
    }

    // ===== 카메라 target 위치 (충돌 처리 전) =====
    const FVector TargetOffset = RemoteArm ? RemoteArm->TargetOffset : FVector::ZeroVector;
    const FVector SocketOffset = RemoteArm ? RemoteArm->SocketOffset : FVector::ZeroVector;
    const FVector ArmOrigin = SmoothedAnchorLocation + TargetOffset;

    // 카메라 위치는 반드시 "스무딩된 회전"에서 파생시킨다.
    // 복제된 카메라 위치(TargetCam.Location)를 직접 쓰면 위치와 회전이 서로 다른 필터를 타게 되고,
    // 시점을 돌리는 동안 카메라가 있는 곳과 보는 곳이 어긋나 화면이 미끄러지듯 흔들린다.
    // (USpringArmComponent 도 같은 순서다: 회전을 먼저 정하고 그 회전으로 팔을 뻗는다)
    const FVector CamTarget = ArmOrigin
        - SmoothedSecondaryRotation.Vector() * SmoothedArmLength
        + FRotationMatrix(SmoothedSecondaryRotation).TransformVector(SocketOffset);

    // ===== Sphere sweep — 벽/지형 뚫림 방지 =====
    // 수신측 자체 환경에서 sweep 하므로 송신측 환경에 의존하지 않음.
    // 스프링암과 동일하게 ArmOrigin -> CamTarget 구간을 쓴다.
    FVector CamLoc = CamTarget;
    if (bDoCollisionTest && World && SmoothedArmLength > KINDA_SMALL_NUMBER)
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SecondaryViewSpringArm), false);
        Params.AddIgnoredActor(RemoteChar);
        // 로컬 플레이어도 ignore (1인칭 시 자기 캡슐 가림 방지)
        if (UGameInstance* GIRef = GetGameInstance())
        {
            if (ULocalPlayer* LP = GIRef->GetFirstGamePlayer())
            {
                if (APlayerController* LocalPC = LP->PlayerController)
                {
                    if (APawn* LocalPawn = LocalPC->GetPawn())
                    {
                        Params.AddIgnoredActor(LocalPawn);
                    }
                }
            }
        }

        FHitResult Hit;
        if (World->SweepSingleByChannel(Hit, ArmOrigin, CamTarget, FQuat::Identity,
            ProbeChannel, FCollisionShape::MakeSphere(ProbeSize), Params))
        {
            CamLoc = Hit.Location;
        }
    }

    // 평활은 Anchor / Rotation 단계에서 이미 끝났다. 여기서 한 번 더 걸면
    // 회전과 위치가 다른 지연을 갖게 되어 시점 회전 중 화면이 흔들린다.
    SmoothedSecondaryLocation = CamLoc;

    UE_LOG(LogCS, VeryVerbose, TEXT("SecondaryView Anchor=%s Rot=%s Arm=%.1f FOV=%.1f -> CamLoc=%s"),
        *SmoothedAnchorLocation.ToCompactString(), *SmoothedSecondaryRotation.ToCompactString(),
        SmoothedArmLength, SmoothedSecondaryFOV, *SmoothedSecondaryLocation.ToCompactString());

#if !UE_BUILD_SHIPPING
    if (World && CVarCSSplitScreenDebug.GetValueOnGameThread() != 0)
    {
        // 노란 구체: 보조 뷰 카메라의 월드 위치
        DrawDebugSphere(World, SmoothedSecondaryLocation, 25.f, 12, FColor::Yellow, false, 0.f, 0, 0.5f);
        // 빨간 선: 카메라 정면 방향 (200 유닛)
        const FVector Forward = SmoothedSecondaryRotation.Vector();
        DrawDebugLine(World, SmoothedSecondaryLocation, SmoothedSecondaryLocation + Forward * 200.f,
            FColor::Red, false, 0.f, 0, 1.f);

        // (1) 받는 쪽 — 보조 카메라에서 가장 가까운 플레이어 폰까지의 거리
        float NearestDist = TNumericLimits<float>::Max();
        FVector NearestCharLoc = FVector::ZeroVector;
        for (FActorIterator It(World); It; ++It)
        {
            APawn* Pawn = Cast<APawn>(*It);
            if (!Pawn || !Pawn->IsPlayerControlled()) continue;
            const float D = FVector::Dist(SmoothedSecondaryLocation, Pawn->GetActorLocation());
            if (D < NearestDist) { NearestDist = D; NearestCharLoc = Pawn->GetActorLocation(); }
        }
        if (NearestDist < TNumericLimits<float>::Max())
        {
            DrawDebugLine(World, SmoothedSecondaryLocation, NearestCharLoc, FColor::Cyan, false, 0.f, 0, 0.5f);
        }

        // (2) 원본 쪽 — 이 머신의 LocalPlayer 의 실제 캐릭터→카메라 거리
        // 이게 SpringArm 충돌-단축 후의 *현재* 값. 이걸 다른 머신이 그대로 받아야 정상.
        APlayerController* LocalPC_ForDiag = nullptr;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (PC->IsLocalController()) { LocalPC_ForDiag = PC; break; }
            }
        }

        float LocalCharCamDist = -1.f;
        if (LocalPC_ForDiag && LocalPC_ForDiag->GetPawn() && LocalPC_ForDiag->PlayerCameraManager)
        {
            const FVector LCharLoc = LocalPC_ForDiag->GetPawn()->GetActorLocation();
            const FVector LCamLoc = LocalPC_ForDiag->PlayerCameraManager->GetCameraCacheView().Location;
            LocalCharCamDist = FVector::Dist(LCharLoc, LCamLoc);

            // 자홍 구체: 로컬 카메라 위치
            DrawDebugSphere(World, LCamLoc, 25.f, 12, FColor::Magenta, false, 0.f, 0, 0.5f);
            // 자홍 선: 로컬 캐릭터 ↔ 로컬 카메라
            DrawDebugLine(World, LCharLoc, LCamLoc, FColor::Magenta, false, 0.f, 0, 1.f);
        }

        if (GEngine)
        {
            const intptr_t BaseKey = reinterpret_cast<intptr_t>(this);
            const FString NetTag = (World->GetNetMode() == NM_Client) ? TEXT("CLIENT") : TEXT("SERVER");

            GEngine->AddOnScreenDebugMessage((int32)(BaseKey + 0), 0.f, FColor::Yellow,
                FString::Printf(TEXT("[%s] SecondaryCam→NearestPawn = %.1f cm  (수신값)"), *NetTag, NearestDist));

            GEngine->AddOnScreenDebugMessage((int32)(BaseKey + 1), 0.f, FColor::Magenta,
                FString::Printf(TEXT("[%s] LocalChar→LocalCam = %.1f cm  (이 머신의 PCM 캐시 = 송신할 값)"),
                    *NetTag, LocalCharCamDist));
        }
    }
#endif

    // FOV 가 정의된 기준 종횡비는 원격 캐릭터의 카메라 컴포넌트에서 읽는다 (기본 16:9).
    // 메인 뷰도 같은 값을 쓰므로 두 뷰의 화각이 일치한다.
    float RemoteAspect = 16.f / 9.f;
    if (const UCameraComponent* RemoteCam = RemoteChar->FindComponentByClass<UCameraComponent>())
    {
        if (RemoteCam->AspectRatio > KINDA_SMALL_NUMBER)
        {
            RemoteAspect = RemoteCam->AspectRatio;
        }
    }

    VC->SetSecondaryView(SmoothedSecondaryLocation, SmoothedSecondaryRotation, SmoothedSecondaryFOV, RemoteAspect);
}

void UCSSplitScreenSubsystem::Tick(float DeltaTime)
{
    // 요청자가 파괴된 채 사라지면(볼륨 삭제, 레벨 언로드) 해제 호출이 오지 않는다.
    // 마지막 요청자가 없어진 순간 스플릿으로 되돌린다 - 그러지 않으면 풀스크린에 갇힌다.
    if (FullScreenRequesters.Num() > 0)
    {
        const int32 Removed = FullScreenRequesters.RemoveAll(
            [](const TWeakObjectPtr<const UObject>& P) { return !P.IsValid(); });

        if (Removed > 0 && FullScreenRequesters.Num() == 0)
        {
            TransitionToSplitScreen();
        }
    }

    if (!bSplitScreenActive)
    {
        return;
    }

    if (!ResolveViewportClient())
    {
        return;
    }

    // 트랜지션 보간
    if (!FMath::IsNearlyEqual(CurrentAlpha, TargetAlpha))
    {
        const float Step = DeltaTime / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER);
        CurrentAlpha = FMath::FInterpConstantTo(CurrentAlpha, TargetAlpha, 1.f, Step);
        if (UCSViewFamilyViewportClient* VC = CachedViewportClient.Get())
        {
            VC->SetFullscreenAlpha(CurrentAlpha);
        }
    }

    // 좌우 배치를 먼저, 그리고 무조건 갱신한다.
    // PushSecondaryCamera 는 프록시/상대 몸/RepCam 미수신에서 조기 반환하므로,
    // 그 안에 두면 배치가 옛 값에 얼어붙는다.
    UpdateSplitSideLayout();

    PushSecondaryCamera();
}

TStatId UCSSplitScreenSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCSSplitScreenSubsystem, STATGROUP_Tickables);
}

bool UCSSplitScreenSubsystem::IsTickable() const
{
    return bEnableSplitScreen && bSplitScreenActive && GetGameInstance() != nullptr;
}

void UCSSplitScreenSubsystem::EnableSplitScreen()
{
    bSplitScreenActive = true;

    // 엔진 기본 split 은 끔 — 우리가 직접 ViewFamily 로 처리
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(true);
    }

    ResolveViewportClient();
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: EnableSplitScreen"));
}

void UCSSplitScreenSubsystem::DisableSplitScreen()
{
    bSplitScreenActive = false;
    if (UCSViewFamilyViewportClient* VC = CachedViewportClient.Get())
    {
        VC->ClearSecondaryView();
    }
    CachedRemoteProxy.Reset();
    LastGoodRemoteBody.Reset();
    SecondaryHoldFrames = 0;
    bLastMainViewOnRight = true;
    bBodySlotWarnLogged = false;
    bHasSmoothedSecondary = false;
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: DisableSplitScreen"));
}

void UCSSplitScreenSubsystem::TransitionToFullScreen(int32 /*PlayerIndex*/)
{
    // 이 함수는 "이 머신의 화면"만 바꾼다. PlayerIndex 는 기존 API 호환을 위해 남겨둔 인자이며 무시된다.
    // 어느 플레이어 때문에 바꿀지는 호출측이 ShouldLocalViewRespondTo() 로 판단해야 한다.
    TargetAlpha = 1.f;
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: TransitionToFullScreen"));
}

void UCSSplitScreenSubsystem::RequestFullScreen(const UObject* Requester)
{
    if (!Requester) return;

    FullScreenRequesters.RemoveAll([](const TWeakObjectPtr<const UObject>& P) { return !P.IsValid(); });
    FullScreenRequesters.AddUnique(TWeakObjectPtr<const UObject>(Requester));

    TransitionToFullScreen(0);
}

void UCSSplitScreenSubsystem::ReleaseFullScreen(const UObject* Requester)
{
    FullScreenRequesters.RemoveAll([Requester](const TWeakObjectPtr<const UObject>& P)
    {
        return !P.IsValid() || P.Get() == Requester;
    });

    if (FullScreenRequesters.Num() == 0)
    {
        TransitionToSplitScreen();
    }
}

bool UCSSplitScreenSubsystem::ShouldLocalViewRespondTo(const ACharacter* Character, bool bForEnteringPlayer, int32 FixedPlayerIndex)
{
    if (!IsValid(Character)) return false;

    if (bForEnteringPlayer)
    {
        // 원격 플레이어의 PlayerController 는 이 머신에 존재하지 않는다.
        // 예전에는 호스트에서 원격 플레이어의 PC 가 잡히고 GetLocalPlayer() 만 null 이라
        // FixedFullScreenPlayerIndex(기본 0) 로 폴백해 "원격 플레이어가 들어갔는데 호스트 화면이 풀스크린"이 됐다.
        return Character->IsLocallyControlled();
    }

    const APlayerController* PC = Cast<APlayerController>(Character->GetController());
    const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
    return LP && LP->GetControllerId() == FixedPlayerIndex;
}

void UCSSplitScreenSubsystem::TransitionToSplitScreen()
{
    TargetAlpha = 0.f;
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: TransitionToSplitScreen"));
}

bool UCSSplitScreenSubsystem::IsInFullScreenMode() const
{
    return TargetAlpha >= 1.f - KINDA_SMALL_NUMBER && CurrentAlpha >= 1.f - KINDA_SMALL_NUMBER;
}
