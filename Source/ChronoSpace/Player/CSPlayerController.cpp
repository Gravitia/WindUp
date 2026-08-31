// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "UI/CSGameUIWidget.h"
#include "Debug/SCSDebugPanel.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Game/CSGameMode.h"
#include "Player/CSPlayerState.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Settings/CSStageDataSettings.h"
#include "Actor/CSStagePortal.h"
#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/InputComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

namespace CSDebugPawnSwap
{
	/** 슬롯을 기획/레벨 쪽 표기(1-based)로 바꾼다. Player0 -> "P1", Player1 -> "P2". */
	static const TCHAR* SlotDisplayName(ECSPlayerSlot Slot)
	{
		return (Slot == ECSPlayerSlot::Player0) ? TEXT("P1") : TEXT("P2");
	}

	/** 슬롯이 둘뿐이라 "반대쪽" 이 곧 상대다. */
	static ECSPlayerSlot GetOtherSlot(ECSPlayerSlot Slot)
	{
		return (Slot == ECSPlayerSlot::Player0) ? ECSPlayerSlot::Player1 : ECSPlayerSlot::Player0;
	}

	/** 스왑 직후 남은 입력 속도로 미끄러지지 않게 세운다 (CSDebugTeleport 의 정리와 같은 취지). */
	static void StopPawnMovement(APawn* InPawn)
	{
		ACharacter* AsCharacter = Cast<ACharacter>(InPawn);
		UCharacterMovementComponent* MovementComp = AsCharacter ? AsCharacter->GetCharacterMovement() : nullptr;
		if (MovementComp == nullptr)
		{
			return;
		}

		MovementComp->StopMovementImmediately();
		MovementComp->Velocity = FVector::ZeroVector;
		MovementComp->UpdateComponentVelocity();
	}
}


ACSPlayerController::ACSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACSPlayerController::ServerRequestStagePortalTravel_Implementation(ACSStagePortal* Portal, int32 Chapter, int32 Stage)
{
	if (Portal)
	{
		Portal->HandleTravelRequest(Chapter, Stage);
	}
}

void ACSPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 디버그 표시 상태는 패널을 띄운 본인만 알면 된다.
	DOREPLIFETIME_CONDITION(ACSPlayerController, bDebugDrivingOtherBody, COND_OwnerOnly);
}

void ACSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetupInputMode();

	// UI 생성을 약간 지연 (PlayerState 초기화 대기)
	GetWorldTimerManager().SetTimer(UICreationTimerHandle, this, &ACSPlayerController::InitializeUI, 0.2f, false);

	UE_LOG(LogCS, Log, TEXT("PlayerController BeginPlay - IsLocalController: %s"),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	// 클라이언트에서 로컬 컨트롤러인 경우 SplitScreen Subsystem 활성화
	// (새 아키텍처: 더미 LocalPlayer / SpectatorPawn 생성 없음 — Subsystem 이 ViewFamily 에 직접 푸시)
	if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
	{
		if (UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			CSSplitSubsystem->EnableSplitScreen();
		}
	}
}

void ACSPlayerController::ShakeCamera()
{
	if (CameraShake)
	{
		ClientStartCameraShake(CameraShake);
	}
}
void ACSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 디버그 패널이 떠 있으면 GameOnly 로 되돌리지 않는다. 패널 버튼으로 캐릭터를 맞바꾸면
	// OnPossess 가 다시 도는데, 여기서 GameOnly 를 걸어 버리면 그 클릭 한 번에 커서와
	// 패널 입력이 같이 죽어 되돌아올 수단이 사라진다.
	if (DebugPanel.IsValid())
	{
		ApplyDebugPanelInputMode();
	}
	else
	{
		SetupInputMode();
	}

	// 레벨 전환 직후엔 체크포인트 안에서 스폰되는 일이 잦은데, 스폰 시점의 폰은 아직 빙의 전이라
	// PlayerState 가 없어 BeginOverlap 이 와도 버려진다. 그 뒤로는 이미 겹친 상태라 새 이벤트도 안 온다.
	// 빙의가 끝난 지금 서 있는 체크포인트를 직접 찾아 적용한다.
	if (HasAuthority())
	{
		// 디버그 캐릭터 스왑은 제외한다. 스왑 후에는 상대 몸에 *내* PlayerState 가 붙는데,
		// 그 상태로 재획득하면 내 개인 리스폰 지점이 상대가 서 있던 곳으로 끌려간다.
		if (!bDebugPossessionSwapInProgress)
		{
			ACSCheckPoint::ClaimCheckPointAtPawnLocation(InPawn);

			// 체크포인트 밖에서 스폰됐으면 리스폰 지점이 계속 null 로 남는다. 가장 가까운 곳으로 채운다.
			ACSRespawnPoint::EnsureRespawnPoint(InPawn);
		}

		// 스왑 여부는 기억하지 않고 매번 다시 판정한다 (헤더의 "사망하면 스왑이 조용히 풀린다" 참고).
		RefreshDebugDrivingOtherBody(InPawn);
	}

	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::ToggleDebugPanel()
{
	if (DebugPanel.IsValid())
	{
		CloseDebugPanel();
		return;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient == nullptr)
	{
		return;
	}

	SAssignNew(DebugPanel, SCSDebugPanel)
		.OwningPlayerController(this);

	// 게임 UI(=100) 보다 위에 얹는다.
	ViewportClient->AddViewportWidgetContent(DebugPanel.ToSharedRef(), 1000);

	// 위젯을 포커스로 잡지 않는다. 그래야 0 키가 계속 PlayerController 로 들어와 다시 닫을 수 있다.
	ApplyDebugPanelInputMode();
}

void ACSPlayerController::ApplyDebugPanelInputMode()
{
	// 패널을 열 때와 빙의가 바뀐 뒤(OnPossess) 양쪽에서 같은 모드를 걸어야 해서 함수로 뺐다.
	// 위젯을 포커스로 잡지 않는 게 핵심이다 — 잡으면 0 키가 위젯으로 먹혀 패널을 못 닫는다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ACSPlayerController::CloseDebugPanel()
{
	if (!DebugPanel.IsValid())
	{
		return;
	}

	if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		ViewportClient->RemoveViewportWidgetContent(DebugPanel.ToSharedRef());
	}

	DebugPanel.Reset();

	// 옵션 메뉴가 떠 있는 상태에서 패널만 닫았다면 그쪽 입력 모드를 건드리지 않는다.
	if (!bIsMenuOpen)
	{
		SetupInputMode();
		bShowMouseCursor = false;
	}
}

namespace CSDebugTeleport
{
	/** 폰 하나를 옮기고 잔여 속도·회전까지 정리한다. RespawnSinglePlayer 와 같은 정리다. */
	static void TeleportPawnForDebug(APlayerController* PC, const FVector& Location, const FRotator& Rotation)
	{
		APawn* TargetPawn = PC ? PC->GetPawn() : nullptr;
		if (!IsValid(TargetPawn))
		{
			return;
		}

		TargetPawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

		// 남아 있던 속도로 그대로 날아가는 걸 막는다.
		if (ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			if (UCharacterMovementComponent* MovementComp = TargetCharacter->GetCharacterMovement())
			{
				MovementComp->StopMovementImmediately();
				MovementComp->Velocity = FVector::ZeroVector;
				MovementComp->SetMovementMode(MOVE_Walking);
				MovementComp->bForceNextFloorCheck = true;
				MovementComp->UpdateComponentVelocity();
			}
		}

		TargetPawn->ForceNetUpdate();
		TargetPawn->FlushNetDormancy();

		PC->SetControlRotation(Rotation);
		PC->ClientSetRotation(Rotation, true);

		if (ACSPlayerController* CSPC = Cast<ACSPlayerController>(PC))
		{
			CSPC->Client_ApplyRespawnView(Rotation);
		}
	}
}

void ACSPlayerController::ServerDebugTeleport_Implementation(FVector Location, FRotator Rotation, bool bAllPlayers)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<APlayerController*> Targets;

	if (bAllPlayers)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				Targets.Add(PC);
			}
		}
	}
	else
	{
		Targets.Add(this);
	}

	// 같은 좌표에 겹쳐 놓으면 캡슐끼리 밀려나 어디로 튈지 모른다. 바라보는 방향 기준 좌우로 벌린다.
	const FVector RightAxis = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
	const float SpreadStep = 150.f;

	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		const float SpreadAmount = (static_cast<float>(Index) - (Targets.Num() - 1) * 0.5f) * SpreadStep;

		CSDebugTeleport::TeleportPawnForDebug(Targets[Index], Location + RightAxis * SpreadAmount, Rotation);
	}

	UE_LOG(LogCS, Log, TEXT("ServerDebugTeleport: %d pawn(s) -> %s"),
		Targets.Num(), *Location.ToCompactString());
}

void ACSPlayerController::ServerDebugSummonPlayer_Implementation(int32 MovingPlayerIndex, int32 AnchorPlayerIndex)
{
	UWorld* World = GetWorld();
	if (World == nullptr || MovingPlayerIndex == AnchorPlayerIndex)
	{
		return;
	}

	// 패널의 P1/P2 는 서버 컨트롤러 순서다 (호스트가 0).
	TArray<APlayerController*> AllPCs;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			AllPCs.Add(PC);
		}
	}

	if (!AllPCs.IsValidIndex(MovingPlayerIndex) || !AllPCs.IsValidIndex(AnchorPlayerIndex))
	{
		UE_LOG(LogCS, Warning, TEXT("ServerDebugSummonPlayer: P%d or P%d not present (%d player(s))"),
			MovingPlayerIndex + 1, AnchorPlayerIndex + 1, AllPCs.Num());
		return;
	}

	const APawn* AnchorPawn = AllPCs[AnchorPlayerIndex]->GetPawn();
	if (!IsValid(AnchorPawn))
	{
		return;
	}

	// 캡슐과 컨트롤 회전에 피치·롤이 섞이면 캐릭터가 눕는다.
	FRotator Rotation = AnchorPawn->GetActorRotation();
	Rotation.Pitch = 0.f;
	Rotation.Roll = 0.f;

	// 같은 좌표에 겹쳐 놓으면 캡슐끼리 밀려난다. 기준 플레이어의 오른쪽 옆에 놓는다.
	const FVector RightAxis = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
	const FVector Location = AnchorPawn->GetActorLocation() + RightAxis * 150.f;

	CSDebugTeleport::TeleportPawnForDebug(AllPCs[MovingPlayerIndex], Location, Rotation);

	UE_LOG(LogCS, Log, TEXT("ServerDebugSummonPlayer: P%d -> P%d at %s"),
		MovingPlayerIndex + 1, AnchorPlayerIndex + 1, *Location.ToCompactString());
}

void ACSPlayerController::ServerDebugTravelToStage_Implementation(int32 Chapter, int32 Stage)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const UCSStageDataSettings* Data = UCSStageDataSettings::Get();
	const FString URL = Data ? Data->GetStageTravelURL(Chapter, Stage) : FString();
	if (URL.IsEmpty())
	{
		UE_LOG(LogCS, Warning, TEXT("ServerDebugTravelToStage: C%d_S%d has no mapped level"), Chapter, Stage);
		return;
	}

	// CSStagePortal 과 달리 클리어 처리는 하지 않는다. 이어하기 위치만 맞춰 둔다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCSGameProgressSubsystem* Progress = GI->GetSubsystem<UCSGameProgressSubsystem>())
		{
			Progress->SetLastPlayedStage(Chapter, Stage);
		}
	}

	UE_LOG(LogCS, Log, TEXT("ServerDebugTravelToStage: C%d_S%d -> %s"), Chapter, Stage, *URL);
	World->ServerTravel(URL, false);
}

int32 ACSPlayerController::SetShowBlueprintCollision(bool bEnable)
{
	// 켜 뒀던 것부터 원래대로 되돌린다. 다시 켤 때 그 사이 스폰된 액터까지 새로 훑기 위해서이기도 하다.
	for (const FCSDebugShapeVisibility& Shown : DebugShownShapes)
	{
		if (UShapeComponent* Shape = Shown.Component.Get())
		{
			Shape->SetHiddenInGame(Shown.bWasHiddenInGame);
			Shape->SetVisibility(Shown.bWasVisible);
		}
	}
	DebugShownShapes.Reset();

	bShowBlueprintCollision = bEnable;

	UWorld* World = GetWorld();
	if (!bEnable || World == nullptr)
	{
		return 0;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		// 블루프린트로 만든 액터만 본다. C++ 액터까지 켜면 show collision 과 다를 게 없어진다.
		if (Cast<UBlueprintGeneratedClass>(Actor->GetClass()) == nullptr)
		{
			continue;
		}

		// Box / Sphere / Capsule 은 전부 UShapeComponent 라 종류를 가릴 필요가 없다.
		TInlineComponentArray<UShapeComponent*> Shapes(Actor);
		for (UShapeComponent* Shape : Shapes)
		{
			if (!IsValid(Shape))
			{
				continue;
			}

			FCSDebugShapeVisibility Shown;
			Shown.Component = Shape;
			Shown.bWasHiddenInGame = Shape->bHiddenInGame != 0;
			Shown.bWasVisible = Shape->GetVisibleFlag();
			DebugShownShapes.Add(Shown);

			Shape->SetHiddenInGame(false);
			Shape->SetVisibility(true);
		}
	}

	UE_LOG(LogCS, Log, TEXT("ShowBlueprintCollision: %d shape(s) shown"), DebugShownShapes.Num());
	return DebugShownShapes.Num();
}

// ============================================================================
// Debug — 다른 플레이어와 조작 캐릭터 맞바꾸기
//
// 전제와 한계(특히 "PlayerState 는 컨트롤러를 따라간다")는 CSPlayerController.h 의
// 같은 이름 섹션에 정리돼 있다. 여기 코드를 고치기 전에 그 주석을 먼저 읽을 것.
//
// 플레이어 사이의 위치 맞추기는 여기 없다 — ServerDebugSummonPlayer 가 이미 그 일을 한다.
// ============================================================================

ACSPlayerController* ACSPlayerController::FindOtherPlayerController() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// 서버에는 접속한 모든 PlayerController 가 있다. 2인 세션이면 나 아닌 하나가 곧 상대다.
	// (클라이언트에서는 자기 PC 만 보이므로 여기서 항상 nullptr 이 나온다 — 그래서 스왑은 Server RPC 다)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ACSPlayerController* PC = Cast<ACSPlayerController>(It->Get());
		if (PC != nullptr && PC != this)
		{
			return PC;
		}
	}

	return nullptr;
}

bool ACSPlayerController::HasOtherPlayerToSwapWith() const
{
	// PlayerController 를 세면 클라이언트에서 항상 1 이 나온다 (남의 PC 는 복제되지 않는다).
	// PlayerState 는 모두에게 복제되므로 GameState 쪽을 센다.
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS != nullptr && GS->PlayerArray.Num() >= 2;
}

ECSPlayerSlot ACSPlayerController::GetOwnPlayerSlot() const
{
	const ACSPlayerState* CSPS = GetPlayerState<ACSPlayerState>();
	return CSPS ? CSPS->GetPlayerSlot() : ECSPlayerSlot::Player0;
}

ECSPlayerSlot ACSPlayerController::GetDrivenBodySlot() const
{
	// 내 슬롯은 스왑해도 안 바뀐다. 그래서 "지금 상대 몸인가" 만 알면 조작 중인 몸이 정해진다.
	const ECSPlayerSlot MySlot = GetOwnPlayerSlot();
	return bDebugDrivingOtherBody ? CSDebugPawnSwap::GetOtherSlot(MySlot) : MySlot;
}

FString ACSPlayerController::GetDebugCurrentBodyName() const
{
	return CSDebugPawnSwap::SlotDisplayName(GetDrivenBodySlot());
}

FString ACSPlayerController::GetDebugSwapTargetName() const
{
	// 버튼을 누르면 지금 아닌 쪽으로 간다. 그 몸이 곧 상대 플레이어가 조작 중인 몸이다.
	return CSDebugPawnSwap::SlotDisplayName(CSDebugPawnSwap::GetOtherSlot(GetDrivenBodySlot()));
}

void ACSPlayerController::SetDebugFixedSplitSide(bool bEnable)
{
	// 상태를 여기 복사해 두지 않는다. 소유자는 서브시스템 뒤의 CVar 하나뿐이다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCSSplitScreenSubsystem* SplitSub = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			SplitSub->SetDebugFixedSplitSide(bEnable);
		}
	}
}

bool ACSPlayerController::IsDebugFixedSplitSide() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UCSSplitScreenSubsystem* SplitSub = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			return SplitSub->IsDebugFixedSplitSide();
		}
	}
	return false;
}

void ACSPlayerController::RefreshDebugDrivingOtherBody(APawn* InPawn)
{
	bDebugDrivingOtherBody = false;

	if (!HasAuthority() || !IsValid(InPawn))
	{
		return;
	}

	// 플래그로 기억하지 않고 몸의 신원으로 판정한다. 사망 리스폰은
	// GetDefaultPawnClassForController 를 타서 내 슬롯의 몸을 다시 만들어 주므로
	// 스왑이 조용히 풀리는데, 기억해 두면 그때 패널 표시가 어긋난다.
	//
	// 판정 규칙은 ACSGameMode::ResolveBodySlotForPawn 한 곳에만 둔다. 분할 화면 좌우 배치도
	// 같은 함수를 쓰므로, 여기서 클래스 비교를 따로 하면 규칙이 둘로 갈라진다.
	bool bSlotResolved = false;
	const ECSPlayerSlot BodySlot = ACSGameMode::ResolveBodySlotForPawn(GetWorld(), InPawn, bSlotResolved);

	bDebugDrivingOtherBody = bSlotResolved && (BodySlot != GetOwnPlayerSlot());
}

ECSPlayerSlot ACSPlayerController::GetSwapTargetBodySlot() const
{
	return CSDebugPawnSwap::GetOtherSlot(GetDrivenBodySlot());
}

void ACSPlayerController::DebugPlayAsP1Body()
{
	// 기획 표기 P1 = 슬롯 Player0.
	ServerDebugPlayAsBodySlot(ECSPlayerSlot::Player0);
}

void ACSPlayerController::DebugPlayAsP2Body()
{
	ServerDebugPlayAsBodySlot(ECSPlayerSlot::Player1);
}

void ACSPlayerController::ServerDebugPlayAsBodySlot_Implementation(ECSPlayerSlot DesiredBodySlot)
{
	// 지금 잡고 있는 몸의 신원을 서버에서 직접 판정한다.
	// 복제되는 bDebugDrivingOtherBody 를 쓰지 않는 이유: 그 값은 OnPossess 뒤에야 갱신되어
	// 클라에 도착하므로, 키를 빠르게 두 번 누르면 옛 값으로 판단해 몸이 도로 튄다.
	bool bSlotResolved = false;
	const ECSPlayerSlot CurrentBodySlot =
		ACSGameMode::ResolveBodySlotForPawn(GetWorld(), GetPawn(), bSlotResolved);

	if (!bSlotResolved)
	{
		// 판정 실패 시 스왑으로 떨어지지 않는다. 그러면 "지정" 이 아니라 토글이 되어
		// 연타할 때 어느 몸에 있게 될지 알 수 없어진다.
		UE_LOG(LogCS, Warning,
			TEXT("DebugPlayAsBodySlot: 지금 몸의 슬롯을 판정하지 못했다 (pawn=%s). "
				 "BP_CSGameMode 의 PawnClassPlayer0/1 이 비었거나 서로 같은지 확인할 것."),
			*GetNameSafe(GetPawn()));
		return;
	}

	if (CurrentBodySlot == DesiredBodySlot)
	{
		// 이미 그 몸이다. 연타해도 안전하도록 조용히 넘긴다.
		return;
	}

	DebugSwapPawnWithOtherPlayer();
}

bool ACSPlayerController::DebugSwapPawnWithOtherPlayer()
{
	ACSPlayerController* OtherPC = FindOtherPlayerController();
	if (OtherPC == nullptr)
	{
		UE_LOG(LogCS, Warning,
			TEXT("DebugSwapPawn: 상대 플레이어가 없다. PIE 를 리슨 서버 + 플레이어 2명으로 실행했는지 확인할 것."));
		return false;
	}

	APawn* MyPawn = GetPawn();
	APawn* OtherPawn = OtherPC->GetPawn();
	if (!IsValid(MyPawn) || !IsValid(OtherPawn) || MyPawn == OtherPawn)
	{
		UE_LOG(LogCS, Warning, TEXT("DebugSwapPawn: 폰이 없다 (mine=%s other=%s)"),
			*GetNameSafe(MyPawn), *GetNameSafe(OtherPawn));
		return false;
	}

	CSDebugPawnSwap::StopPawnMovement(MyPawn);
	CSDebugPawnSwap::StopPawnMovement(OtherPawn);

	// 두 컨트롤러 모두 스왑 중으로 표시한다. OnPossess 가 양쪽에서 각각 돌기 때문이다.
	bDebugPossessionSwapInProgress = true;
	OtherPC->bDebugPossessionSwapInProgress = true;

	// 반드시 둘 다 먼저 놓고 나서 교차로 잡는다.
	// 한쪽씩 Possess 하면 AController::OnPossess 의 "이미 남이 잡고 있으면 뺏는다" 경로를 타서
	// 상대 컨트롤러가 폰 없는 상태로 남고, 그 사이 프레임에 카메라가 튄다.
	UnPossess();
	OtherPC->UnPossess();

	Possess(OtherPawn);
	OtherPC->Possess(MyPawn);

	bDebugPossessionSwapInProgress = false;
	OtherPC->bDebugPossessionSwapInProgress = false;

	// 서버 쪽 ControlRotation 은 APlayerController::OnPossess 가 폰 회전으로 맞춰 준다.
	// 하지만 클라이언트는 자기 시선을 스스로 들고 있어서, 그대로 두면 갈아탄 몸에서 엉뚱한
	// 방향을 보고 있다. 폰의 액터 회전을 그대로 넘긴다 — 커스텀 중력으로 몸이 기울어 있으면
	// 그 기울기까지 따라가는 게 맞다 (yaw 만 남기면 카메라가 지면과 어긋난다).
	ClientSetRotation(OtherPawn->GetActorRotation(), /*bResetCamera=*/true);
	OtherPC->ClientSetRotation(MyPawn->GetActorRotation(), /*bResetCamera=*/true);

	UE_LOG(LogCS, Log, TEXT("DebugSwapPawn: %s <-> %s (%s <-> %s)"),
		*GetName(), *OtherPC->GetName(), *GetNameSafe(MyPawn), *GetNameSafe(OtherPawn));

	return true;
}

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);

	CloseDebugPanel();
	DebugShownShapes.Reset();

	if (GameUIWidget)
	{
		GameUIWidget->RemoveFromParent();
		GameUIWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ACSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsValid(InputComponent))
		return;

	InputComponent->BindAction("ToggleOption", IE_Pressed, this, &ACSPlayerController::ToggleOption);

	// 상단 숫자열 0. 넘버패드 0 은 EKeys::NumPadZero 라 여기에 걸리지 않는다.
	// 쉬핑 빌드에서도 쓸 수 있도록 의도적으로 가드 없이 열어 둔다.
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ACSPlayerController::ToggleDebugPanel);

	// 상단 숫자열 8 / 9 — 조작할 몸을 직접 고른다. 패널을 열지 않고 바로 오갈 수 있어야
	// 혼자 2인 테스트할 때 반복이 편하다. 토글이 아니라 지정이라 연타해도 안전하다.
	// (넘버패드는 EKeys::NumPadEight / NumPadNine 이라 여기에 걸리지 않는다)
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ACSPlayerController::DebugPlayAsP1Body);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ACSPlayerController::DebugPlayAsP2Body);
}

void ACSPlayerController::SetupInputMode()
{
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void ACSPlayerController::InitializeUI()
{
	if (IsLocalPlayerController())
	{
		ShowGameUI();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				TEXT("Game UI Initialized for Local Player"));
		}
	}
}

void ACSPlayerController::CreateGameUI()
{
	if (GameUIWidgetClass && !GameUIWidget && IsLocalPlayerController())
	{
		GameUIWidget = CreateWidget<UCSGameUIWidget>(this, GameUIWidgetClass);

		if (GEngine && GameUIWidget)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue,
				TEXT("Game UI Widget Created"));
		}
	}
}

void ACSPlayerController::ShowGameUI()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	CreateGameUI();

	if (GameUIWidget)
	{
		if (!GameUIWidget->IsInViewport())
		{
			GameUIWidget->AddToViewport();

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
					TEXT("Game UI Added to Viewport"));
			}
		}

		GameUIWidget->SetVisibility(ESlateVisibility::Visible);
		RefreshGameUI();
	}
}

void ACSPlayerController::HideGameUI()
{
	if (GameUIWidget)
	{
		GameUIWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ACSPlayerController::ToggleGameUI()
{
	if (IsGameUIVisible())
	{
		HideGameUI();
	}
	else
	{
		ShowGameUI();
	}
}

void ACSPlayerController::RefreshGameUI()
{
	if (GameUIWidget)
	{
		GameUIWidget->RefreshPlayerUI();
	}
}

bool ACSPlayerController::IsGameUIVisible() const
{
	return GameUIWidget &&
		GameUIWidget->IsInViewport() &&
		GameUIWidget->GetVisibility() == ESlateVisibility::Visible;
}

// ============================================================
// Zoom RPC — 호환성 유지용 빈 구현
// ============================================================
// 새 ViewFamily 분할 화면에서는 PlayerCameraManager 의 결과 카메라 위치(SpringArm 적용 후)가
// 그대로 RepCam.Location 으로 리플리케이트되므로, 별도의 ArmLength 동기화가 필요 없다.
// CSCameraZoomComponent 가 호출하는 시그니처를 유지하기 위해 본문은 비워둔다.

void ACSPlayerController::ServerBroadcastZoomToOthers_Implementation(float /*NewArmLength*/)
{
}

void ACSPlayerController::ClientSetSpectatorCameraArmLength_Implementation(float /*NewArmLength*/)
{
}

void ACSPlayerController::Client_ApplyRespawnView_Implementation(const FRotator& Rot)
{
	SetControlRotation(Rot);

	if (APawn* P = GetPawn())
	{
		P->SetActorRotation(Rot);
	}
}

void ACSPlayerController::ToggleOption()
{
	check(OptionWidgetClass != nullptr);

	if (bIsMenuOpen) CloseOption();
	else OpenOption();
}

void ACSPlayerController::OpenOption()
{
	OptionWidget = CreateWidget(this, OptionWidgetClass);

	if (OptionWidget == nullptr)
		return;

	OptionWidget->AddToViewport(100);

	FInputModeGameAndUI UIInputMode;
	UIInputMode.SetWidgetToFocus(OptionWidget->TakeWidget());
	SetInputMode(UIInputMode);
	bShowMouseCursor = true;
	bIsMenuOpen = true;
}

void ACSPlayerController::CloseOption()
{
	if (OptionWidget == nullptr)
		return;

	OptionWidget->RemoveFromParent();
	OptionWidget = nullptr;

	SetupInputMode();
	bShowMouseCursor = false;
	bIsMenuOpen = false;
}
