// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CSPlayerController.generated.h"

class UCSGameUIWidget;
class SCSServerTravelWidget;
class ACSStagePortal;

/**
 *
 */
UCLASS()
class CHRONOSPACE_API ACSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACSPlayerController();

	// UI 위젯 클래스 - Blueprint에서 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UCSGameUIWidget> GameUIWidgetClass;

	// 게임 UI 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TObjectPtr<UCSGameUIWidget> GameUIWidget;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshGameUI();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsGameUIVisible() const;

	void ShakeCamera();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraShake")
	TSubclassOf<class UCameraShakeBase> CameraShake;

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	virtual void OnRep_PlayerState() override;

private:
	void SetupInputMode();
	void CreateGameUI();
	void InitializeUI();
	FTimerHandle UICreationTimerHandle;

// Camera Zoom Sync (CSCameraZoomComponent 호환 — 새 ViewFamily 분할 화면에서는 카메라 위치 자체가
//                   리플리케이트되므로 SpringArm 길이 동기화는 더 이상 필요 없다. 호출자 호환을 위해
//                   시그니처만 유지하며, 실제 구현은 빈 본문이다.)
public:
	UFUNCTION(Server, Reliable)
	void ServerBroadcastZoomToOthers(float NewArmLength);

	UFUNCTION(Client, Reliable)
	void ClientSetSpectatorCameraArmLength(float NewArmLength);

// Respawn Camera Direction Set
public:
	UFUNCTION(Client, Reliable)
	void Client_ApplyRespawnView(const FRotator& Rot);

// Stage Portal — forwards a client's stage-select choice to the (server-side) portal,
// which is a world actor the client cannot RPC to directly.
public:
	UFUNCTION(Server, Reliable)
	void ServerRequestStagePortalTravel(ACSStagePortal* Portal, int32 Chapter, int32 Stage);

// Debug — 체크포인트 순간이동 패널 (상단 숫자열 0)
public:
	/** 패널을 열거나 닫는다. 0 키와 패널의 X 버튼이 여기로 들어온다. */
	void ToggleDebugTeleportPanel();
	void CloseDebugTeleportPanel();

	/** 이동은 서버 권한이다. 패널을 누른 쪽이 클라이언트여도 서버가 옮긴다. */
	UFUNCTION(Server, Reliable)
	void ServerDebugTeleport(FVector Location, FRotator Rotation, bool bAllPlayers);

private:
	TSharedPtr<class SCSDebugTeleportPanel> DebugTeleportPanel;

// Option Menu
public:
	void ToggleOption();
	void OpenOption();
	virtual void CloseOption();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> OptionWidgetClass;

	UPROPERTY()
	TObjectPtr<class UUserWidget> OptionWidget;

	bool bIsMenuOpen;
};
