// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/CSGameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ChronoSpace.h"

void UCSGameViewportClient::LayoutPlayers()
{
	// 기본 레이아웃 먼저 실행 (엔진 기본 배치)
	Super::LayoutPlayers();

	// 2인 스플릿 스크린일 때만 처리
	const int32 NumPlayers = GetOuterUEngine()->GetNumGamePlayers(GetWorld());
	if (NumPlayers != 2) return;

	ULocalPlayer* Player0 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 0);
	ULocalPlayer* Player1 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 1);

	if (!Player0 || !Player1) return;

	// ── 풀스크린 모드 또는 전환 중 ──
	if (bIsFullScreen || bTransitioning)
	{
		// 풀스크린 대상과 상대 결정
		ULocalPlayer* FullPlayer = (FullScreenPlayerIndex == 0) ? Player0 : Player1;
		ULocalPlayer* OtherPlayer = (FullScreenPlayerIndex == 0) ? Player1 : Player0;

		// 스플릿 상태에서의 기본 위치 (좌우 교체 적용)
		// Player0(본인)은 오른쪽(0.5, 0), Player1(상대)은 왼쪽(0, 0)
		FVector2D FullPlayerSplitOrigin = (FullScreenPlayerIndex == 0) ? FVector2D(0.5f, 0.f) : FVector2D(0.f, 0.f);
		FVector2D OtherPlayerSplitOrigin = (FullScreenPlayerIndex == 0) ? FVector2D(0.f, 0.f) : FVector2D(0.5f, 0.f);
		FVector2D SplitSize(0.5f, 1.f);

		// 풀스크린 목표: 대상=전체화면, 상대=너비만 0으로 (세로선 따라 왼쪽으로 밀기)
		FVector2D FullOriginTarget(0.f, 0.f);
		FVector2D FullSizeTarget(1.f, 1.f);
		FVector2D OtherSizeTarget(0.f, 1.f);	// 높이 유지, 너비만 0

		// EaseInOut 커브 적용
		const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, TransitionAlpha, 2.f);

		// Lerp 적용
		FullPlayer->Origin = FMath::Lerp(FullPlayerSplitOrigin, FullOriginTarget, SmoothedAlpha);
		FullPlayer->Size = FMath::Lerp(SplitSize, FullSizeTarget, SmoothedAlpha);

		OtherPlayer->Origin = OtherPlayerSplitOrigin;	// Origin 고정 (왼쪽 그대로)
		OtherPlayer->Size = FMath::Lerp(SplitSize, OtherSizeTarget, SmoothedAlpha);

		return;
	}

	// ── 일반 스플릿 스크린 — 기존 좌우 교체 로직 ──
	FVector2D Origin0 = Player0->Origin;
	FVector2D Size0   = Player0->Size;

	FVector2D Origin1 = Player1->Origin;
	FVector2D Size1   = Player1->Size;

	// 교환: Player 0(본인) → 기존 Player 1 위치, Player 1(상대방) → 기존 Player 0 위치
	Player0->Origin = Origin1;
	Player0->Size   = Size1;

	Player1->Origin = Origin0;
	Player1->Size   = Size0;

	UE_LOG(LogCS, Verbose, TEXT("LayoutPlayers: Swapped viewports - P0(%s) <-> P1(%s)"),
		*Origin1.ToString(), *Origin0.ToString());
}

void UCSGameViewportClient::StartFullScreenTransition(int32 PlayerIndex)
{
	if (bIsFullScreen && FullScreenPlayerIndex == PlayerIndex)
	{
		UE_LOG(LogCS, Log, TEXT("Already in full screen mode for Player %d"), PlayerIndex);
		return;
	}

	FullScreenPlayerIndex = PlayerIndex;
	bTransitioning = true;
	bToFullScreen = true;
	TransitionAlpha = 0.f;

	// 기존 타이머 정리 후 시작
	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
		CurrentWorld->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			FTimerDelegate::CreateUObject(this, &UCSGameViewportClient::UpdateTransitionLerp),
			0.016f,
			true
		);
	}

	UE_LOG(LogCS, Log, TEXT("StartFullScreenTransition: Player %d, Duration %f"), PlayerIndex, TransitionDuration);
}

void UCSGameViewportClient::StartSplitScreenTransition()
{
	if (!bIsFullScreen && !bTransitioning)
	{
		UE_LOG(LogCS, Log, TEXT("Already in split screen mode"));
		return;
	}

	bTransitioning = true;
	bToFullScreen = false;
	TransitionAlpha = 1.f;	// 풀스크린 상태에서 스플릿으로 돌아감

	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
		CurrentWorld->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			FTimerDelegate::CreateUObject(this, &UCSGameViewportClient::UpdateTransitionLerp),
			0.016f,
			true
		);
	}

	UE_LOG(LogCS, Log, TEXT("StartSplitScreenTransition: Restoring split screen"));
}

void UCSGameViewportClient::UpdateTransitionLerp()
{
	const float DeltaAlpha = 0.016f / FMath::Max(TransitionDuration, 0.01f);

	if (bToFullScreen)
	{
		TransitionAlpha = FMath::Clamp(TransitionAlpha + DeltaAlpha, 0.f, 1.f);

		if (TransitionAlpha >= 1.f)
		{
			// 전환 완료: 풀스크린 진입
			bTransitioning = false;
			bIsFullScreen = true;

			if (UWorld* CurrentWorld = GetWorld())
			{
				CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
			}

			UE_LOG(LogCS, Log, TEXT("Full screen transition complete for Player %d"), FullScreenPlayerIndex);
		}
	}
	else
	{
		TransitionAlpha = FMath::Clamp(TransitionAlpha - DeltaAlpha, 0.f, 1.f);

		if (TransitionAlpha <= 0.f)
		{
			// 전환 완료: 스플릿 스크린 복귀
			bTransitioning = false;
			bIsFullScreen = false;

			if (UWorld* CurrentWorld = GetWorld())
			{
				CurrentWorld->GetTimerManager().ClearTimer(TransitionTimerHandle);
			}

			UE_LOG(LogCS, Log, TEXT("Split screen transition complete"));
		}
	}
}
