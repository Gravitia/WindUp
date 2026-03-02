// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/CSLobbyPlayerController.h"

void ACSLobbyPlayerController::Client_ShowLobbyImage_Implementation()
{
	// 블루프린트에서 구현한 OnShowLobbyImage 호출
	OnShowLobbyImage();
}

void ACSLobbyPlayerController::Client_HideLobbyImage_Implementation()
{
	// 블루프린트에서 구현한 OnHideLobbyImage 호출
	OnHideLobbyImage();
}

void ACSLobbyPlayerController::Client_SwitchLobbyWidget_Implementation(int32 NewIndex)
{
	// 블루프린트에서 구현한 OnSwitchLobbyWidget 호출
	OnSwitchLobbyWidget(NewIndex);
}
