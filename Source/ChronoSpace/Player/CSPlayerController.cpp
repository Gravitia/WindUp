// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "UI/CSGameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/CSGameMode.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "ChronoSpace.h"


ACSPlayerController::ACSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
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

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);

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
