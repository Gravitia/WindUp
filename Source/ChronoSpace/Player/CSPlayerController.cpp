// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "UI/CSGameUIWidget.h"
#include "Debug/SCSDebugTeleportPanel.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Game/CSGameMode.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
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
#include "ChronoSpace.h"


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

	SetupInputMode();

	// 레벨 전환 직후엔 체크포인트 안에서 스폰되는 일이 잦은데, 스폰 시점의 폰은 아직 빙의 전이라
	// PlayerState 가 없어 BeginOverlap 이 와도 버려진다. 그 뒤로는 이미 겹친 상태라 새 이벤트도 안 온다.
	// 빙의가 끝난 지금 서 있는 체크포인트를 직접 찾아 적용한다.
	if (HasAuthority())
	{
		ACSCheckPoint::ClaimCheckPointAtPawnLocation(InPawn);

		// 체크포인트 밖에서 스폰됐으면 리스폰 지점이 계속 null 로 남는다. 가장 가까운 곳으로 채운다.
		ACSRespawnPoint::EnsureRespawnPoint(InPawn);
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

void ACSPlayerController::ToggleDebugTeleportPanel()
{
#if !UE_BUILD_SHIPPING
	if (DebugTeleportPanel.IsValid())
	{
		CloseDebugTeleportPanel();
		return;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient == nullptr)
	{
		return;
	}

	SAssignNew(DebugTeleportPanel, SCSDebugTeleportPanel)
		.OwningPlayerController(this);

	// 게임 UI(=100) 보다 위에 얹는다.
	ViewportClient->AddViewportWidgetContent(DebugTeleportPanel.ToSharedRef(), 1000);

	// 위젯을 포커스로 잡지 않는다. 그래야 0 키가 계속 PlayerController 로 들어와 다시 닫을 수 있다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
#endif
}

void ACSPlayerController::CloseDebugTeleportPanel()
{
#if !UE_BUILD_SHIPPING
	if (!DebugTeleportPanel.IsValid())
	{
		return;
	}

	if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		ViewportClient->RemoveViewportWidgetContent(DebugTeleportPanel.ToSharedRef());
	}

	DebugTeleportPanel.Reset();

	// 옵션 메뉴가 떠 있는 상태에서 패널만 닫았다면 그쪽 입력 모드를 건드리지 않는다.
	if (!bIsMenuOpen)
	{
		SetupInputMode();
		bShowMouseCursor = false;
	}
#endif
}

void ACSPlayerController::ServerDebugTeleport_Implementation(FVector Location, FRotator Rotation, bool bAllPlayers)
{
#if !UE_BUILD_SHIPPING
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
		APlayerController* PC = Targets[Index];
		APawn* TargetPawn = PC ? PC->GetPawn() : nullptr;
		if (!IsValid(TargetPawn))
		{
			continue;
		}

		const float SpreadAmount = (static_cast<float>(Index) - (Targets.Num() - 1) * 0.5f) * SpreadStep;

		TargetPawn->SetActorLocationAndRotation(
			Location + RightAxis * SpreadAmount,
			Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		// RespawnSinglePlayer 와 같은 정리. 남아 있던 속도로 그대로 날아가는 걸 막는다.
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

	UE_LOG(LogCS, Log, TEXT("ServerDebugTeleport: %d pawn(s) -> %s"),
		Targets.Num(), *Location.ToCompactString());
#endif
}

int32 ACSPlayerController::SetShowBlueprintCollision(bool bEnable)
{
#if !UE_BUILD_SHIPPING
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
#else
	return 0;
#endif
}

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);

	CloseDebugTeleportPanel();
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

#if !UE_BUILD_SHIPPING
	// 상단 숫자열 0. 넘버패드 0 은 EKeys::NumPadZero 라 여기에 걸리지 않는다.
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ACSPlayerController::ToggleDebugTeleportPanel);
#endif
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
