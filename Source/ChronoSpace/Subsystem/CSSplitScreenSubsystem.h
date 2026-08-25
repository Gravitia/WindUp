// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CSSplitScreenSubsystem.generated.h"

class ACSCameraViewProxy;
class ACSCharacterPlayer;
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

    /**
     * 풀스크린 "요청"을 등록/해제한다. 요청자가 하나라도 남아 있으면 풀스크린을 유지하고,
     * 전부 사라졌을 때만 스플릿으로 돌아간다.
     *
     * 트리거들이 TransitionToFullScreen/TransitionToSplitScreen 을 직접 부르면
     * 볼륨 A 에서 볼륨 B 로 겹쳐 이동할 때 A 의 EndOverlap 이 B 의 BeginOverlap 보다 늦게 도착해
     * B 가 막 켠 풀스크린을 A 가 꺼버린다 (반대 순서면 나온 뒤에도 풀스크린이 남는다).
     * 요청자가 파괴돼도 자동으로 정리된다.
     */
    void RequestFullScreen(const UObject* Requester);
    void ReleaseFullScreen(const UObject* Requester);

    /**
     * 이 머신의 화면이 이 캐릭터의 진입/이탈에 반응해야 하는지 판단한다.
     * TransitionToFullScreen 은 로컬 뷰만 바꾸므로, 원격 플레이어 때문에 내 화면이 바뀌면 안 된다.
     * bForEnteringPlayer=false 면 FixedPlayerIndex 와 로컬 플레이어의 ControllerId 를 비교한다.
     */
    static bool ShouldLocalViewRespondTo(const class ACharacter* Character, bool bForEnteringPlayer, int32 FixedPlayerIndex);

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

    /** OLD 시스템 (RInterpTo 45 + SpringArm RotationLag 60) 의 직렬 합성과 동등한 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float RotationInterpSpeed = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float FOVInterpSpeed = 12.f;

    /** SpringArm 길이 보간 속도 — 줌 인/아웃 시 카메라가 부드럽게 따라옴 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float ArmLengthInterpSpeed = 30.f;

    /**
     * Anchor (원격 캐릭터 위치) 보간 속도.
     * UE 의 CharacterMovement NetSmoothing 은 *Mesh* 에만 적용되고 Capsule 은 step-up 점프함.
     * 이 보간이 그 step 을 흡수해 캐릭터 이동 시 떨림을 제거한다.
     * 60 = 약 16ms lag (거의 즉시).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float AnchorInterpSpeed = 60.f;

    /** 새 RepCam 도착 시 위치 차이가 이 값(cm)을 넘으면 보간 없이 즉시 스냅 (텔레포트/리스폰 대응) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Smoothing")
    float SnapDistance = 1500.f;

    /** 보조 뷰 카메라 충돌 처리 (벽/지형 뚫림 방지) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Collision")
    bool bDoCollisionTest = true;

    /** Sphere sweep 반지름 (USpringArmComponent::ProbeSize 기본값과 동일) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Collision")
    float ProbeSize = 12.f;

    /** Sweep 채널 — OLD 시스템과 동일한 ECC_Camera 가 기본. 필요시 CCHANNEL_CSSPECTATOR 로 변경 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen|Collision")
    TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bSplitScreenActive = false;

private:
    /** 캐싱된 ViewportClient (GameViewportClientClassName 이 UCSViewFamilyViewportClient 일 때만 유효) */
    TWeakObjectPtr<UCSViewFamilyViewportClient> CachedViewportClient;

    /** 다른 플레이어의 카메라 정보를 들고 있는 Proxy (로컬 플레이어 시점에서 *상대*) */
    TWeakObjectPtr<ACSCameraViewProxy> CachedRemoteProxy;

    /** 원격 캐릭터 — anchor 위치 (CharacterMovement Replication NetSmoothing 적용된 부드러운 신호) */
    TWeakObjectPtr<ACSCharacterPlayer> CachedRemoteCharacter;

    /** Fullscreen 트랜지션 상태 — 0=split, 1=fullscreen */
    float CurrentAlpha = 0.f;
    float TargetAlpha = 0.f;

    // 풀스크린을 요청 중인 트리거들. 비면 스플릿으로 복귀한다 (파괴된 요청자는 자동 정리)
    TArray< TWeakObjectPtr<const UObject> > FullScreenRequesters;

    /** 보조 뷰 카메라 보간 상태 */
    bool bHasSmoothedSecondary = false;
    FVector  SmoothedAnchorLocation = FVector::ZeroVector;
    FVector  SmoothedSecondaryLocation = FVector::ZeroVector;
    FRotator SmoothedSecondaryRotation = FRotator::ZeroRotator;
    float    SmoothedSecondaryFOV = 90.f;
    float    SmoothedArmLength = 0.f;

    /** ViewportClient 캐싱 시도 — 실패 시 false */
    bool ResolveViewportClient();

    /** 로컬에서 *상대* 가 되는 Proxy 를 찾음 (NetMode 별 분기) */
    ACSCameraViewProxy* ResolveRemoteProxy() const;

    /** 로컬에서 *상대* 캐릭터를 찾음 (anchor 용) */
    ACSCharacterPlayer* ResolveRemoteCharacter() const;

    /** 매 프레임 보조 뷰 카메라 푸시 */
    void PushSecondaryCamera();
};
