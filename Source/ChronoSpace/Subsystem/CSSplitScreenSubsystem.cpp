// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSSplitScreenSubsystem.h"
#include "UI/CSGameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "ChronoSpace.h"

UCSSplitScreenSubsystem::UCSSplitScreenSubsystem()
{
    // 기본값 설정
    bEnableSplitScreen = true;
    bUseDualMode = true;
    MaxSplitScreenPlayers = 2;
    bSplitScreenActive = false;
}

void UCSSplitScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (bEnableSplitScreen)
    {
        SetupSplitScreenViewport();
    }
}

void UCSSplitScreenSubsystem::SetupSplitScreenViewport()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogTemp, Error, TEXT("SS Cannot setup viewport - GEngine or GameViewport is null"));
        return;
    }

    // 스플릿 스크린을 항상 활성화
    GEngine->GameViewport->SetForceDisableSplitscreen(false);
    GEngine->GameViewport->MaxSplitscreenPlayers = MaxSplitScreenPlayers;

    // 듀얼 모드 설정
    if (bUseDualMode)
    {
        GEngine->GameViewport->MaxSplitscreenPlayers = 2;
        UE_LOG(LogCS, Log, TEXT("Dual Mode Viewport Setup - Max Players: 2"));
    }
    else
    {
        UE_LOG(LogCS, Log, TEXT("Standard Viewport Setup - Max Players: %d"), MaxSplitScreenPlayers);
    }
}

void UCSSplitScreenSubsystem::EnableSplitScreen()
{
    if (bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split Screen already enabled"));
        return;
    }

    SetupSplitScreenViewport();
    bSplitScreenActive = true;

    FString ModeString = bUseDualMode ? TEXT("Dual Mode") : TEXT("Standard Mode");
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Enabled - %s"), *ModeString);
}

void UCSSplitScreenSubsystem::DisableSplitScreen()
{
    if (!bSplitScreenActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Split Screen already disabled"));
        return;
    }

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(true);
    }

    bSplitScreenActive = false;
    UE_LOG(LogTemp, Warning, TEXT("SS Split Screen Disabled"));
}

void UCSSplitScreenSubsystem::TransitionToFullScreen(int32 PlayerIndex)
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogCS, Warning, TEXT("TransitionToFullScreen: GameViewport is null"));
        return;
    }

    UCSGameViewportClient* ViewportClient = Cast<UCSGameViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        UE_LOG(LogCS, Warning, TEXT("TransitionToFullScreen: Not using CSGameViewportClient"));
        return;
    }

    ViewportClient->StartFullScreenTransition(PlayerIndex);
    UE_LOG(LogCS, Log, TEXT("TransitionToFullScreen: Requested for Player %d"), PlayerIndex);
}

void UCSSplitScreenSubsystem::TransitionToSplitScreen()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogCS, Warning, TEXT("TransitionToSplitScreen: GameViewport is null"));
        return;
    }

    UCSGameViewportClient* ViewportClient = Cast<UCSGameViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        UE_LOG(LogCS, Warning, TEXT("TransitionToSplitScreen: Not using CSGameViewportClient"));
        return;
    }

    ViewportClient->StartSplitScreenTransition();
    UE_LOG(LogCS, Log, TEXT("TransitionToSplitScreen: Requested"));
}

bool UCSSplitScreenSubsystem::IsInFullScreenMode() const
{
    if (!GEngine || !GEngine->GameViewport)
    {
        return false;
    }

    UCSGameViewportClient* ViewportClient = Cast<UCSGameViewportClient>(GEngine->GameViewport);
    if (!ViewportClient)
    {
        return false;
    }

    return ViewportClient->IsInFullScreenMode();
}
