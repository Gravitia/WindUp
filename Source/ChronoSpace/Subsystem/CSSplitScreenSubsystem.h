// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CSSplitScreenSubsystem.generated.h"

class ACSCameraViewProxy;
class UCSViewFamilyViewportClient;

/**
 * ViewFamily 기반 분할 화면 컨트롤러.
 *
 * 매 프레임 다른 플레이어의 카메라(Loc/Rot/FOV)를 ACSCameraViewProxy 에서 읽어
 * UCSViewFamilyViewportClient 에 푸시한다. Split↔Fullscreen 전환은 ViewRect 의
 * 너비를 Lerp 하여 구현 (별도 더미 LocalPlayer / SpectatorPawn 불필요).
 */
UCLASS()
class CHRONOSPACE_API UCSSplitScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    UCSSplitScreenSubsystem();

    // UGameInstanceSubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual bool IsTickableInEditor() const override { return false; }

    // ── Public API ──
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void DisableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void TransitionToFullScreen(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void TransitionToSplitScreen();

    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsInFullScreenMode() const;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen", meta = (ClampMin = "0.05", ClampMax = "5.0"))
    float TransitionDuration = 0.5f;

    /** 보조 뷰 카메라 보간 속도 (FInterpTo Speed). 클수록 빠르게 따라잡고 작을수록 부드럽다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float LocationInterpSpeed = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float RotationInterpSpeed = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float FOVInterpSpeed = 12.f;

    /** 새 RepCam 도착 시 위치 차이가 이 값(cm)을 넘으면 보간 없이 즉시 스냅 (텔레포트/리스폰 대응) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float SnapDistance = 1500.f;

    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bSplitScreenActive = false;

private:
    /** 캐싱된 ViewportClient (GameViewportClientClassName 이 UCSViewFamilyViewportClient 일 때만 유효) */
    TWeakObjectPtr<UCSViewFamilyViewportClient> CachedViewportClient;

    /** 다른 플레이어의 카메라 정보를 들고 있는 Proxy (로컬 플레이어 시점에서 *상대*) */
    TWeakObjectPtr<ACSCameraViewProxy> CachedRemoteProxy;

    /** Fullscreen 트랜지션 상태 — 0=split, 1=fullscreen */
    float CurrentAlpha = 0.f;
    float TargetAlpha = 0.f;

    /** 보조 뷰 카메라 보간 상태 */
    bool bHasSmoothedSecondary = false;
    FVector  SmoothedSecondaryLocation = FVector::ZeroVector;
    FRotator SmoothedSecondaryRotation = FRotator::ZeroRotator;
    float    SmoothedSecondaryFOV = 90.f;

    /** ViewportClient 캐싱 시도 — 실패 시 false */
    bool ResolveViewportClient();

    /** 로컬에서 *상대* 가 되는 Proxy 를 찾음 (NetMode 별 분기) */
    ACSCameraViewProxy* ResolveRemoteProxy() const;

    /** 매 프레임 보조 뷰 카메라 푸시 */
    void PushSecondaryCamera();
};
