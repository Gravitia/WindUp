// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/CSGameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "ChronoSpace.h"

void UCSGameViewportClient::LayoutPlayers()
{
	// 기본 레이아웃 먼저 실행 (엔진 기본 배치)
	Super::LayoutPlayers();

	// 2인 스플릿 스크린일 때만 좌우 교체
	const int32 NumPlayers = GetOuterUEngine()->GetNumGamePlayers(GetWorld());
	if (NumPlayers != 2) return;

	// 두 LocalPlayer를 가져와서 SubRect를 서로 교환
	ULocalPlayer* Player0 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 0);
	ULocalPlayer* Player1 = GetOuterUEngine()->GetGamePlayer(GetWorld(), 1);

	if (!Player0 || !Player1) return;

	// 현재 뷰포트 영역 저장
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
