// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CSLobbyPlayerController.generated.h"

/**
 * 로비 전용 PlayerController
 * - 로비에서는 플레이어 Pawn이 존재하지 않음
 * - Client RPC로 서버 → 클라이언트 UI 표시/숨김 지원
 * - BlueprintImplementableEvent로 실제 UI 처리를 블루프린트에 위임
 */
UCLASS()
class CHRONOSPACE_API ACSLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ===== Client RPC =====

	/** 서버 → 특정 클라이언트: 로비 이미지 위젯 표시 */
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Lobby|UI")
	void Client_ShowLobbyImage();

	/** 서버 → 특정 클라이언트: 로비 이미지 위젯 숨김 */
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Lobby|UI")
	void Client_HideLobbyImage();

	/** 서버 → 특정 클라이언트: Widget Switcher 인덱스 변경 */
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Lobby|UI")
	void Client_SwitchLobbyWidget(int32 NewIndex);

	// ===== Blueprint에서 오버라이드할 이벤트 =====

	/** 블루프린트에서 이미지 위젯을 Visible로 만드는 로직 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|UI")
	void OnShowLobbyImage();

	/** 블루프린트에서 이미지 위젯을 Hidden/Collapsed로 만드는 로직 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|UI")
	void OnHideLobbyImage();

	/** 블루프린트에서 Widget Switcher의 Active Index를 변경하는 로직 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|UI")
	void OnSwitchLobbyWidget(int32 NewIndex);
};
