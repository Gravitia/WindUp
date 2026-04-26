// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSSplitScreenSubsystem.h"

#include "UI/CSViewFamilyViewportClient.h"
#include "Actor/CSCameraViewProxy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ChronoSpace.h"

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
    Super::Deinitialize();
}

bool UCSSplitScreenSubsystem::ResolveViewportClient()
{
    if (CachedViewportClient.IsValid())
    {
        return true;
    }
    if (!GEngine || !GEngine->GameViewport)
    {
        return false;
    }
    UCSViewFamilyViewportClient* VC = Cast<UCSViewFamilyViewportClient>(GEngine->GameViewport);
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
        // 상대 Proxy 가 아직 도착하지 않음 → 보조 뷰 비활성
        VC->ClearSecondaryView();
        return;
    }

    const FRepCamInfo& Cam = Proxy->GetReplicatedCamera();

    // 유효성 체크 — Location 이 모두 0 이면 아직 미수신으로 간주
    if (Cam.Location.IsNearlyZero() && Cam.Rotation.IsNearlyZero())
    {
        VC->ClearSecondaryView();
        return;
    }

    VC->SetSecondaryView(Cam.Location, Cam.Rotation, Cam.FOV);
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
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: DisableSplitScreen"));
}

void UCSSplitScreenSubsystem::TransitionToFullScreen(int32 /*PlayerIndex*/)
{
    // 현재 구현은 메인(로컬) 뷰만 풀스크린으로 확장. PlayerIndex 는 기존 API 호환 위해 시그니처 유지.
    TargetAlpha = 1.f;
    UE_LOG(LogCS, Log, TEXT("CSSplitScreenSubsystem: TransitionToFullScreen"));
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
