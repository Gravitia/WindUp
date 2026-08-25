// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "CSViewFamilyViewportClient.generated.h"

/**
 * Secondary view 를 직접 ViewFamily 에 주입하는 커스텀 GameViewportClient.
 *
 * 기존 구조 (더미 LocalPlayer + DummyPlayerController + SpectatorPawn) 를 제거하고,
 * 단일 LocalPlayer 위에서 두 번째 SceneView 를 ViewFamily 안에 직접 추가하여 분할 화면을 구현한다.
 *
 * - Secondary 카메라는 외부 (Subsystem) 가 매 프레임 SetSecondaryView() 로 푸시
 * - bSecondaryActive == false 인 동안에는 엔진 기본 Draw 로 폴백 (싱글 뷰)
 * - bSwapLeftRight == true 이면 메인 뷰가 우측, 보조 뷰가 좌측
 * - FullscreenAlpha 0..1 로 Split↔Fullscreen 전환 (메인 뷰의 ViewRect 너비 Lerp)
 */
UCLASS()
class CHRONOSPACE_API UCSViewFamilyViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	UCSViewFamilyViewportClient(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	virtual void BeginDestroy() override;

	/** Subsystem 이 매 프레임 호출 — 보조 뷰 카메라 갱신 */
	void SetSecondaryView(const FVector& InLocation, const FRotator& InRotation, float InFOV, float InAspectRatio = 16.f / 9.f);

	/** 보조 뷰 비활성화 → 엔진 기본 Draw 로 폴백 */
	void ClearSecondaryView();

	bool IsSecondaryViewActive() const { return bSecondaryActive; }

	/** 0=split (50/50), 1=메인 뷰가 화면 전체 */
	void SetFullscreenAlpha(float InAlpha) { FullscreenAlpha = FMath::Clamp(InAlpha, 0.f, 1.f); }
	float GetFullscreenAlpha() const { return FullscreenAlpha; }

	/** true 면 메인=우측 / 보조=좌측 (기존 구현 호환). 기본 true. */
	void SetSwapLeftRight(bool bInSwap) { bSwapLeftRight = bInSwap; }

	/** 메인/보조 뷰 위에 동시에 적용되는 검정 페이드. PlayerCameraManager 와 독립. */
	void StartFade(float TargetAlpha, float Duration);
	float GetFadeAlpha() const { return FadeAlpha; }

private:
	/** 좌/우 뷰 사각형 계산 (FullscreenAlpha 적용 + Swap) */
	void ComputeViewRects(const FIntPoint& ViewportSize, FIntRect& OutMainRect, FIntRect& OutSecondaryRect) const;

	/** 보조 뷰의 SceneView 를 ViewFamily 에 추가 */
	class FSceneView* AddSecondarySceneView(class FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect);

	/** 엔진 Draw 가 매 프레임 하던 오디오 리스너 갱신 (분할 경로에서 빠져 있었다) */
	void UpdateAudioListener(class UWorld* ListenerWorld, class APlayerController* PC, const class FSceneView* View);

	/** 분할이 끝나면 LocalPlayer 의 Origin/Size 를 전체 뷰포트로 되돌린다 */
	void RestoreMainLocalPlayerViewport();

	/** HUD 를 그릴 씬 캔버스 (엔진의 CanvasObject 대응) */
	UPROPERTY(Transient)
	TObjectPtr<class UCanvas> SceneCanvasObject = nullptr;

private:
	bool bSecondaryActive = false;

	FVector  SecondaryLocation = FVector::ZeroVector;
	FRotator SecondaryRotation = FRotator::ZeroRotator;
	float    SecondaryFOV = 90.f;

	// SecondaryFOV 가 "가로 FOV" 로 정의된 기준 종횡비. 송신측 카메라 컴포넌트의 AspectRatio (기본 16:9).
	float    SecondaryAspectRatio = 16.f / 9.f;

	/** 분할 경계를 이 픽셀 배수로 스냅한다 (TSR 타일 / ScreenPercentage 반올림 정렬용) */
	static constexpr int32 ViewRectAlignment = 8;



	float FullscreenAlpha = 0.f;
	bool  bSwapLeftRight = true;

	/** 보조 뷰의 ViewState (TSR / Lumen ScreenProbe / AutoExposure 히스토리) */
	FSceneViewStateReference SecondaryViewState;

	/** HUD / Console / OnScreenDebug 등 PostRender 단계용 캐시 UCanvas */
	UPROPERTY(Transient)
	TObjectPtr<class UCanvas> DebugCanvasObject;

	void TickFade(float DeltaSeconds);
	void DrawFadeOverlay(FViewport* InViewport);
	void OnPostLoadMap(UWorld* LoadedWorld);

	float FadeAlpha = 0.f;
	float FadeTargetAlpha = 0.f;
	float FadeSpeed = 1.f;
	FDelegateHandle PostLoadMapHandle;
};
