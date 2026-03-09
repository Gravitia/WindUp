// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "CSGameViewportClient.generated.h"

/**
 * 스플릿 스크린 좌우 교체 및 풀스크린 전환 애니메이션을 위한 커스텀 GameViewportClient.
 */
UCLASS()
class CHRONOSPACE_API UCSGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	/** Split Screen → Full Screen 전환 시작 (특정 플레이어를 풀스크린으로) */
	void StartFullScreenTransition(int32 PlayerIndex);

	/** Full Screen → Split Screen 전환 시작 */
	void StartSplitScreenTransition();

	/** 현재 풀스크린 모드인지 (전환 중 포함) */
	bool IsInFullScreenMode() const { return bIsFullScreen || bTransitioning; }

protected:
	/** 2인 스플릿 스크린 레이아웃을 오버라이드하여 좌우를 교체합니다. */
	virtual void LayoutPlayers() override;

private:
	/** Lerp 업데이트 (타이머 콜백) */
	void UpdateTransitionLerp();

	// ── 전환 상태 ──
	bool bIsFullScreen = false;
	bool bTransitioning = false;
	bool bToFullScreen = false;			// true: Split→Full, false: Full→Split
	int32 FullScreenPlayerIndex = 0;	// 풀스크린 대상 플레이어

	float TransitionAlpha = 0.f;		// 0=Split, 1=FullScreen

	/** 전환 애니메이션 시간 (초) */
	float TransitionDuration = 0.5f;

	FTimerHandle TransitionTimerHandle;
};
