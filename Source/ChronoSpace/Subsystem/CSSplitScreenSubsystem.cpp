// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSSplitScreenSubsystem.h"

#include "UI/CSViewFamilyViewportClient.h"
#include "Actor/CSCameraViewProxy.h"
#include "Character/CSCharacterPlayer.h"
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
    CachedRemoteCharacter.Reset();
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

ACSCharacterPlayer* UCSSplitScreenSubsystem::ResolveRemoteCharacter() const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return nullptr;

    // 원격 플레이어 캐릭터 = IsLocallyControlled() == false 인 ACSCharacterPlayer
    // CharacterMovement Replication 의 NetSmoothing 이 적용된 부드러운 위치 신호를 anchor 로 사용
    for (TActorIterator<ACSCharacterPlayer> It(World); It; ++It)
    {
        ACSCharacterPlayer* Char = *It;
        if (!Char) continue;
        if (Char->IsLocallyControlled()) continue;
        return Char;
    }
    return nullptr;
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

    // 원격 캐릭터 (anchor) 찾기
    ACSCharacterPlayer* RemoteChar = CachedRemoteCharacter.Get();
    if (!RemoteChar)
    {
        RemoteChar = ResolveRemoteCharacter();
        if (RemoteChar)
        {
            CachedRemoteCharacter = RemoteChar;
        }
    }
    if (!RemoteChar)
    {
        VC->ClearSecondaryView();
        bHasSmoothedSecondary = false;
        return;
    }

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
    // USpringArmComponent::UpdateDesiredArmLocation 과 같은 순서로 재구성한다.
    //   ArmOrigin = 부착점 + TargetOffset
    //   Desired   = ArmOrigin - Rot.Vector() * ArmLength + Rot 로 회전한 SocketOffset
    // 예전엔 TargetOffset / SocketOffset 을 빼먹어서, 어깨 오프셋을 쓰는 카메라(3인칭 표준)에서
    // 보조 뷰만 캐릭터가 화면 중앙에 오는 등 메인 뷰와 구도가 어긋났다.
    // (오프셋 값은 클래스 기본값이라 모든 머신에서 같으므로 복제 없이 로컬에서 읽어도 된다)
    const FVector TargetOffset = RemoteArm ? RemoteArm->TargetOffset : FVector::ZeroVector;
    const FVector SocketOffset = RemoteArm ? RemoteArm->SocketOffset : FVector::ZeroVector;

    const FVector ArmOrigin = SmoothedAnchorLocation + TargetOffset;
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
    CachedRemoteCharacter.Reset();
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
