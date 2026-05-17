// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/CSLobbyGameMode.h"
#include "Player/CSLobbyPlayerController.h"

ACSLobbyGameMode::ACSLobbyGameMode()
{
	PlayerControllerClass = ACSLobbyPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = false;
}

void ACSLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
}
