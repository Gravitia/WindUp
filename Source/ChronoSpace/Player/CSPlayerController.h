// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CSPlayerController.generated.h"

class UCSGameUIWidget;
class SCSServerTravelWidget;
class ACSStagePortal;

/** 디버그로 켜 준 셰이프 컴포넌트의 원래 표시 상태. 끌 때 그대로 되돌리기 위해 들고 있는다. */
struct FCSDebugShapeVisibility
{
	TWeakObjectPtr<class UShapeComponent> Component;
	bool bWasHiddenInGame = true;
	bool bWasVisible = true;
};

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

	/**
	 * MovingPlayerIndex 플레이어를 AnchorPlayerIndex 플레이어 옆으로 옮긴다.
	 * 인덱스는 서버 컨트롤러 순서다 (0 = 호스트 = P1, 1 = 클라이언트 = P2).
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugSummonPlayer(int32 MovingPlayerIndex, int32 AnchorPlayerIndex);

	/**
	 * UCSStageDataSettings 에서 레벨을 찾아 즉시 ServerTravel 한다.
	 * CSStagePortal 과 달리 클리어 처리·페이드 없이 바로 이동하는 디버그 전용 경로다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugTravelToStage(int32 Chapter, int32 Stage);

	/**
	 * 블루프린트 액터에 달린 콜리전 셰이프(Box/Sphere/Capsule)를 게임 화면에 그린다.
	 * show collision 과 달리 블루프린트로 만든 액터만 골라내므로 트리거만 보기에 좋다.
	 * 순수하게 보는 쪽 연출이라 로컬에서만 처리한다 — 서버로 보내지 않는다.
	 * @return 표시로 바꾼 셰이프 개수
	 */
	int32 SetShowBlueprintCollision(bool bEnable);
	bool IsShowingBlueprintCollision() const { return bShowBlueprintCollision; }

private:
	TSharedPtr<class SCSDebugTeleportPanel> DebugTeleportPanel;

	TArray<FCSDebugShapeVisibility> DebugShownShapes;
	bool bShowBlueprintCollision = false;

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
