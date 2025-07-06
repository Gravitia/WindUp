// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "ChronoSpace.h"
#include "UI/SCSServerTravelWidget.h"
#include "UI/CSGameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACSPlayerController::ACSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

}

void ACSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	check(CameraShake);

	SetupInputMode();

	// UI 생성을 약간 지연 (PlayerState 초기화 대기)
	GetWorldTimerManager().SetTimer(UICreationTimerHandle, this, &ACSPlayerController::InitializeUI, 0.2f, false);
}

void ACSPlayerController::ShakeCamera()
{
	ClientStartCameraShake(CameraShake);
}
void ACSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetupInputMode();

	// Pawn이 변경될 때 UI 새로고침
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState가 복제될 때 UI 새로고침
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);

	// UI 정리
	if (GameUIWidget)
	{
		GameUIWidget->RemoveFromParent();
		GameUIWidget = nullptr;
	}

	// Slate UI 정리
	if (ServerTravelWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ServerTravelWidget.ToSharedRef());
		ServerTravelWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ACSPlayerController::SetupInputMode()
{
	// 게임 전용 입력 모드 설정
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void ACSPlayerController::InitializeUI()
{
	// 로컬 플레이어 컨트롤러에서만 UI 생성
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
	// 로컬 플레이어에서만 UI 표시
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

		// UI 새로고침
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


void ACSPlayerController::CreateServerTravelWidget()
{
	// 로컬 플레이어에서만 생성
	if (!IsLocalPlayerController())
	{
		return;
	}

	// 이미 생성되어 있으면 패스
	if (ServerTravelWidget.IsValid())
	{
		return;
	}

	// Slate 위젯 생성
	ServerTravelWidget = SNew(SCSServerTravelWidget);

	if (GEngine && ServerTravelWidget.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			TEXT("Server Travel Widget Created"));
	}
}

void ACSPlayerController::ShowServerTravelUI()
{
	// 로컬 플레이어에서만 실행
	if (!IsLocalPlayerController())
	{
		return;
	}

	// 위젯이 없으면 생성
	CreateServerTravelWidget();

	// GameViewport에 추가
	if (ServerTravelWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			ServerTravelWidget.ToSharedRef(),
			1000  // Z-Order (높을수록 앞에 표시)
		);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
				TEXT("Server Travel UI Shown"));
		}
	}
}

void ACSPlayerController::HideServerTravelUI()
{
	if (ServerTravelWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ServerTravelWidget.ToSharedRef());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
				TEXT("Server Travel UI Hidden"));
		}
	}
}

void ACSPlayerController::ToggleServerTravelUI()
{
	if (IsServerTravelUIVisible())
	{
		HideServerTravelUI();
	}
	else
	{
		ShowServerTravelUI();
	}
}

bool ACSPlayerController::IsServerTravelUIVisible() const
{
	// Slate 위젯의 가시성은 GameViewport에 추가되어 있는지로 판단
	// 정확한 체크를 위해서는 별도의 bool 변수를 관리하는 것이 좋음
	return ServerTravelWidget.IsValid();
}