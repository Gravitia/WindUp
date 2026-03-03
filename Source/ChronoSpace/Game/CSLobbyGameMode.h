// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CSLobbyGameMode.generated.h"

/**
 * 로비 전용 GameMode
 * - PlayerControllerClass를 CSLobbyPlayerController로 지정
 * - 로비에서 필요한 UI RPC를 지원
 */
UCLASS()
class CHRONOSPACE_API ACSLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACSLobbyGameMode();
};
