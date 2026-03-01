// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "CSGameViewportClient.generated.h"

/**
 * 스플릿 스크린 좌우 교체를 위한 커스텀 GameViewportClient.
 * Player 0(본인)을 오른쪽, Player 1(상대방)을 왼쪽에 배치합니다.
 */
UCLASS()
class CHRONOSPACE_API UCSGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

protected:
	/** 2인 스플릿 스크린 레이아웃을 오버라이드하여 좌우를 교체합니다. */
	virtual void LayoutPlayers() override;
};
