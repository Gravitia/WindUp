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

    /**
     * 디버그: 분할 화면의 좌우를 *조작 중인 캐릭터* 에 고정한다 — P1 몸 왼쪽, P2 몸 오른쪽.
     *
     * 기본 동작(false)은 "메인 뷰(내가 조작 중인 몸)를 항상 오른쪽에 둔다" 라서,
     * 두 창을 나란히 놓고 보면 같은 자리에 서로 다른 캐릭터가 떠 헷갈린다.
     * 켜면 두 창의 좌우 배치가 같아진다. 실제로 바뀌는 건 P1 몸을 조작 중인 창뿐이다
     * (P2 몸을 조작 중인 창은 기본값과 결과가 같다).
     *
     * 비셰이핑 빌드에서는 기본으로 켜져 있다. 사용자가 설정 없이 "그냥" 되기를 원했기 때문이다.
     * 상태는 프로세스 전역 콘솔 변수 cs.SplitScreen.FixedSide 하나가 소유한다 —
     * PIE 두 창이 같은 값을 보므로 창마다 따로 켤 필요가 없다. 끄려면 체크박스나 CVar 를 쓴다.
     * 셰이핑에서는 컴파일 타임에 꺼져 런타임에 켤 방법이 없다.
     */
    void SetDebugFixedSplitSide(bool bEnable);
    bool IsDebugFixedSplitSide() const;

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

    /** 이 머신의 로컬 ACSPlayerController. 없으면 nullptr. */
    class ACSPlayerController* ResolveLocalPlayerController() const;

    /**
     * 이 창이 조작 중인 몸과 상대가 조작 중인 몸을 한 번에 찾는다. 둘을 따로 구하지 않는 것이 핵심이다 —
     * 따로 구하면 "양쪽 절반이 같은 몸" 상태가 조용히 만들어질 수 있다.
     *
     * 상대는 GameState->PlayerArray 에서 내 PlayerState 가 아닌 것의 GetPawn() 이다.
     *   - APawn::PlayerState 는 무조건 복제되고 APawn::OnRep_PlayerState 가
     *     APlayerState::GetPawn() 역포인터를 클라에서도 유지한다 -> 호스트와 클라가 같은 답을 낸다.
     *   - 빙의가 바뀌면 APawn::PossessedBy 가 PlayerState 를 옮기므로 캐릭터 스왑을 자동으로 따라간다.
     *   - PlayerState 가 없는 ACSCharacterPlayer 파생(미믹/디코이)은 후보에 오르지 않는다.
     *     예전 방식(TActorIterator + !IsLocallyControlled())은 컨트롤러 없는 그 인형들을
     *     "원격 캐릭터" 로 잘못 집었다.
     *
     * @return 상대 몸을 찾았으면 true
     */
    bool ResolveSplitViewPair(ACSCharacterPlayer*& OutLocalBody, ACSCharacterPlayer*& OutRemoteBody) const;

    /** 좌우 배치 갱신. PushSecondaryCamera 와 분리되어 있어야 그쪽 조기 반환에 걸려 얼어붙지 않는다. */
    void UpdateSplitSideLayout();

    /** 좌우 고정 배치가 지금 유효한가 (셰이핑에서는 컴파일 타임에 false). */
    bool IsFixedSideLayoutActive() const;

    /** 매 프레임 보조 뷰 카메라 푸시 */
    void PushSecondaryCamera();

    /**
     * 직전 프레임의 보조 뷰 대상. *대상 결정에는 쓰지 않는다* — 아래 두 가지에만 쓴다.
     *   1) 대상이 바뀐 프레임을 감지해 보간 상태를 리셋 (안 하면 카메라가 옛 몸에서 새 몸으로 미끄러진다)
     *   2) 스왑 직후 PlayerState 역포인터가 한두 프레임 어긋날 때의 짧은 유예
     * 예전의 CachedRemoteCharacter 처럼 "캐시가 있으면 재해석 안 함" 으로 쓰면 안 된다.
     * 그게 스왑 후 양쪽 절반이 같은 몸으로 보이던 원인이었다.
     */
    TWeakObjectPtr<ACSCharacterPlayer> LastGoodRemoteBody;
    int32 SecondaryHoldFrames = 0;

    /** 좌우 배치 마지막 값. 판정에 실패하면 추측하지 않고 이 값을 유지한다. */
    bool bLastMainViewOnRight = true;

    /** 몸 슬롯 판정 실패 경고를 한 번만 찍기 위한 래치. */
    bool bBodySlotWarnLogged = false;
};
