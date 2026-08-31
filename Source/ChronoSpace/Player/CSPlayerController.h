// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/CSPlayerState.h" // ECSPlayerSlot
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void SetupInputMode();

	/**
	 * 디버그 패널이 떠 있는 동안 쓰는 입력 모드(GameAndUI + 커서 표시).
	 *
	 * OnPossess 가 원래 무조건 SetupInputMode()(=GameOnly) 를 부르는데, 디버그 캐릭터 스왑은
	 * 패널 버튼 클릭으로 OnPossess 를 다시 돌린다. 그때 GameOnly 가 걸리면 그 클릭 한 번에
	 * 커서와 패널 입력이 같이 죽어서 되돌아올 수단이 사라진다.
	 */
	void ApplyDebugPanelInputMode();

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
	void ToggleDebugPanel();
	void CloseDebugPanel();

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
	TSharedPtr<class SCSDebugPanel> DebugPanel;

	TArray<FCSDebugShapeVisibility> DebugShownShapes;
	bool bShowBlueprintCollision = false;

// ============================================================================
// Debug — 다른 플레이어와 조작 캐릭터 맞바꾸기
//   (디버그 패널 SCSDebugPanel 의 "Two Player Debug" 버튼이 여기로 들어온다)
//
// ┌─ 전제: PIE 를 "리슨 서버 + 플레이어 2명" 으로 실행한다 ─────────────────────────
// │
// │ 이 기능은 없던 몸을 만들지 않는다. 두 플레이어가 처음부터 정상 경로로 접속해
// │ 각자 PlayerController / PlayerState / 슬롯 / 폰을 온전히 갖고 있는 상태에서,
// │ *조작 대상만* 서로 맞바꾼다. 그래서 다음이 전부 그냥 동작한다:
// │   - 두 몸 모두 컨트롤러가 있다 -> CharacterMovement 가 정상 시뮬레이션된다.
// │     (컨트롤러 없는 몸은 PerformMovement 자체를 건너뛰어 중력도 블랙홀 임펄스도 안 먹는다)
// │   - 두 몸 모두 PlayerState 가 있다 -> APawn::IsPlayerControlled() 가 참이라
// │     ACSKillZone / ACSCheckPoint / ACSStagePortal 류가 정상적으로 반응한다.
// │   - 두 몸 모두 ASC 아바타가 잡혀 있다 -> GAS 가 양쪽 다 산다.
// │   - ACSCameraViewProxy 가 양쪽에 생겨 있다 -> 분할 화면이 정상 경로로 뜬다.
// │
// │ [단 하나 어긋나는 것 — PlayerState 는 컨트롤러를 따라간다]
// │   APawn::PossessedBy 가 Pawn->SetPlayerState(Controller->PlayerState) 를 하므로,
// │   스왑 후 상대 몸에는 *내* PlayerState 가 붙는다. 즉 몸의 겉모습(폰 클래스)은 P2 인데
// │   슬롯은 내 것(P1)으로 읽힌다. 슬롯으로 분기하는 로직은 이 점을 알고 봐야 한다.
// │
// │   PlayerState 를 같이 맞바꾸지 않는 것은 의도된 선택이다. UCSPlayerSlotSubsystem 은
// │   PlayerState 의 UniqueNetId 로 슬롯을 영속 배정하는데, 컨트롤러와 PlayerState 의
// │   짝을 바꾸면 그 키가 뒤섞여 다음 레벨에서 두 사람의 캐릭터가 통째로 뒤바뀐다.
// │   ASC 의 Mixed 리플리케이션 소유 커넥션도 같이 어긋난다. 디버그 편의보다 손해가 크다.
// │
// │ [사망하면 스왑이 조용히 풀린다]
// │   ACSGameMode::RespawnSinglePlayer 는 GetDefaultPawnClassForController 를 타므로
// │   내 슬롯의 몸을 다시 만들어 준다. 그래서 표시 상태를 플래그로 기억하지 않고
// │   OnPossess 에서 ACSGameMode::ResolveBodySlotForPawn 으로 매번 다시 판정한다.
// │
// │ [셰이핑 가드 없음]
// │   디버그 창을 셰이핑에서도 열 수 있게 한 팀 결정(커밋 "shipping시 디버그창 안뜨는거
// │   뜨게 가드 변경")을 따른다. 이 섹션도 #if !UE_BUILD_SHIPPING 으로 감싸지 않는다 —
// │   감싸면 같은 패널 안에서 어떤 버튼은 되고 어떤 버튼은 안 되는 엇박이 된다.
// └──────────────────────────────────────────────────────────────────────────────
public:
	/**
	 * 지정한 슬롯의 *몸* 을 조작한다. 이미 그 몸을 잡고 있으면 아무것도 하지 않는다.
	 *
	 * 토글이 아니라 "지정" 이다. 8/9 키를 연타하거나 두 사람이 동시에 눌러도 몸이 튀지 않는다.
	 * 실제로 일어나는 일은 두 컨트롤러가 폰을 맞바꾸는 것이라, 상대의 조작 대상도 같이 바뀐다.
	 * 빙의는 서버 권한이므로 키를 누른 게 클라이언트여도 서버가 처리한다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugPlayAsBodySlot(ECSPlayerSlot DesiredBodySlot);

	/**
	 * 패널 활성화용 — 세션에 플레이어가 둘 이상인가.
	 * PlayerController 가 아니라 GameState 의 PlayerArray 를 센다. 클라이언트에는 남의
	 * PlayerController 가 복제되지 않지만 PlayerState 는 모두에게 복제되기 때문이다.
	 */
	bool HasOtherPlayerToSwapWith() const;

	/** 패널 라벨용 — 지금 조작 중인 몸의 표시 이름 ("P1" / "P2"). */
	FString GetDebugCurrentBodyName() const;

	/** 패널 라벨용 — 버튼을 누르면 조작하게 될 몸의 표시 이름 ("P1" / "P2"). */
	FString GetDebugSwapTargetName() const;

	/** 패널 버튼용 — 지금 조작 중이 *아닌* 쪽 몸의 슬롯. ServerDebugPlayAsBodySlot 의 인자가 된다. */
	ECSPlayerSlot GetSwapTargetBodySlot() const;

	/**
	 * 화면 절반을 *몸* 에 고정한다 — P1 몸 왼쪽, P2 몸 오른쪽.
	 *
	 * 셰이핑 동작은 "두 사람 모두 자기 몸을 오른쪽에서 본다" 라, 두 창을 나란히 놓고 보면
	 * 같은 자리에 서로 다른 캐릭터가 떠서 헷갈린다. 고정하면 두 창의 좌우가 같아지고,
	 * 캐릭터를 맞바꿔도 몸이 자기 자리를 지킨다.
	 *
	 * 상태는 프로세스 전역 CVar cs.SplitScreen.FixedSide 하나가 소유한다
	 * (PIE 두 창에 동시에 적용된다). 여기 함수들은 그 CVar 창구일 뿐 자체 상태를 들고 있지 않다 —
	 * 사본을 두면 체크박스와 실제 배치가 어긋난다.
	 */
	void SetDebugFixedSplitSide(bool bEnable);
	bool IsDebugFixedSplitSide() const;

	/**
	 * 지금 조작 중인 *몸* 의 슬롯. 스왑 여부(bDebugDrivingOtherBody)를 반영한 값이다.
	 * 내 PlayerState 슬롯과는 다를 수 있다 — 그게 이 기능의 요점이다.
	 *
	 * 디버그 패널 라벨 전용이다. 분할 화면 좌우 배치에는 쓰지 말 것 —
	 * 이 값은 서버 OnPossess 에서만 갱신되는 COND_OwnerOnly 복제라 클라에서 왕복 지연만큼 늦다.
	 * 배치는 UCSSplitScreenSubsystem 이 ACSGameMode::ResolveBodySlotForPawn 으로 직접 판정한다.
	 */
	ECSPlayerSlot GetDrivenBodySlot() const;

private:
	/**
	 * 상단 숫자열 8 / 9. 각각 P1 몸 / P2 몸을 조작한다.
	 * 패널을 열지 않고 바로 오갈 수 있어야 반복 테스트가 편하다.
	 */
	void DebugPlayAsP1Body();
	void DebugPlayAsP2Body();

	/** 두 컨트롤러가 폰을 맞바꾸는 본체. 서버에서만 부를 것. @return 실제로 바꿨으면 true */
	bool DebugSwapPawnWithOtherPlayer();

	/** 세션의 다른 ACSPlayerController. 서버에서만 의미가 있다 (클라는 남의 PC 를 모른다). */
	ACSPlayerController* FindOtherPlayerController() const;

	/** 지금 잡은 몸이 내 슬롯의 몸인지 서버가 다시 판정해 bDebugDrivingOtherBody 를 갱신한다. */
	void RefreshDebugDrivingOtherBody(APawn* InPawn);

	/** 내 PlayerState 슬롯. 캐릭터를 맞바꿔도 이 값은 안 바뀐다. PlayerState 가 없으면 Player0. */
	ECSPlayerSlot GetOwnPlayerSlot() const;

	/**
	 * 내 원래 몸이 아니라 상대 몸을 조작 중인가. 서버가 OnPossess 에서 갱신하고 소유 클라에 복제한다.
	 * 패널 라벨이 이걸 읽는다.
	 */
	UPROPERTY(Replicated)
	bool bDebugDrivingOtherBody = false;

	/** 스왑이 도는 동안 OnPossess 의 체크포인트/리스폰 지점 재획득을 막는다. 서버 전용. */
	bool bDebugPossessionSwapInProgress = false;

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
