// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "UI/CSGameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/CSGameMode.h"
#include "TimerManager.h"
#include "ChronoSpace.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Character/CSCharacterPlayer.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"



ACSPlayerController::ACSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	check(CameraShake && RenderTargetP0 && RenderTargetP1);

	SetupInputMode();

	// UI 생성을 약간 지연 (PlayerState 초기화 대기)
	GetWorldTimerManager().SetTimer(UICreationTimerHandle, this, &ACSPlayerController::InitializeUI, 0.2f, false);

	if (IsLocalController())
	{
		int32 Width = 960;
		int32 Height = 1080;

		FVector2D ScreenResolution;

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ScreenResolution);
			Width = FMath::Max(Width, ScreenResolution.X / 2);
			Height = FMath::Max(Height, ScreenResolution.Y / 2);
		}

		RenderTargetP0->ResizeTarget(Width, Height);
		RenderTargetP1->ResizeTarget(Width, Height);
		RenderTargetP0->UpdateResourceImmediate(true);
		RenderTargetP1->UpdateResourceImmediate(true);

		if (HasAuthority())
		{
			ACSGameMode* GameMode = Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->OnPlayerLogin.AddDynamic(this, &ACSPlayerController::UpdateRenderTarget);
		}

		UpdateRenderTarget();
	}
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

	// PlayerState 동기화 이후에도 다시 시도
	UpdateRenderTarget();
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

	Super::EndPlay(EndPlayReason);
}

void ACSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("DualMode", IE_Pressed, this, &ACSPlayerController::ToggleDualMode);
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

void ACSPlayerController::UpdateRenderTarget()
{
	ACSCharacterPlayer* MyCharacter = Cast<ACSCharacterPlayer>(GetPawn());
	ACSCharacterPlayer* OtherCharacter = FindFirstOtherPawn();

	if (!MyCharacter || !OtherCharacter)
	{
		// 다른 Pawn이 아직 없으면 0.5초 뒤에 재시도
		GetWorldTimerManager().SetTimerForNextTick(this, &ACSPlayerController::UpdateRenderTarget);
		return;
	}

	// 항상 내 캐릭터는 오른쪽, 상대는 왼쪽
	ASceneCapture2D* CapLeft = SpawnCaptureAndAttach(OtherCharacter->GetFollowCamera(), RenderTargetP0);
	ASceneCapture2D* CapRight = SpawnCaptureAndAttach(MyCharacter->GetFollowCamera(), RenderTargetP1);

	if (CapLeft)
	{
		UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UpdateRenderTarget - Left=Other"), GetWorld()->GetNetMode());
	}
	if (CapRight)
	{
		UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UpdateRenderTarget - Right=Self"), GetWorld()->GetNetMode());
	}
}

ACSCharacterPlayer* ACSPlayerController::FindFirstOtherPawn()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return nullptr;

	for (TActorIterator<ACSCharacterPlayer> It(GetWorld()); It; ++It)
	{
		ACSCharacterPlayer* Candidate = *It;
		if (Candidate != MyPawn)
		{
			return Candidate;
		}
	}

	return nullptr;
}

ASceneCapture2D* ACSPlayerController::SpawnCaptureAndAttach(UCameraComponent* TargetCam, UTextureRenderTarget2D* TargetRT)
{
	if (!TargetCam || !TargetRT) return nullptr;

	FActorSpawnParameters Params;
	Params.Owner = this;

	ASceneCapture2D* CaptureActor = GetWorld()->SpawnActor<ASceneCapture2D>(
		ASceneCapture2D::StaticClass(),
		FTransform::Identity,
		Params
	);

	if (CaptureActor)
	{
		CaptureActor->SetReplicates(false); // 로컬 전용
	}

	if (!CaptureActor) return nullptr;

	USceneCaptureComponent2D* CaptureComp = CaptureActor->GetCaptureComponent2D();
	CaptureComp->TextureTarget = TargetRT;
	CaptureComp->CaptureSource = SCS_FinalColorLDR;
	CaptureComp->bCaptureEveryFrame = true;

	CaptureActor->AttachToComponent(Cast<USceneComponent>(TargetCam), FAttachmentTransformRules::SnapToTargetIncludingScale);

	return CaptureActor;
}

void ACSPlayerController::ToggleDualMode()
{
	if (bIsDualMode) CloseDualMode();
	else OpenDualMode();
}

void ACSPlayerController::OpenDualMode()
{
	check(DualModeUIClass);

	if (!DualModeUI)
	{
		DualModeUI = CreateWidget(this, DualModeUIClass);
	}

	DualModeUI->AddToViewport(-100);

	if (CaptureP0)
	{
		CaptureP0->GetCaptureComponent2D()->SetComponentTickEnabled(true);
	}
	if (CaptureP1)
	{
		CaptureP1->GetCaptureComponent2D()->SetComponentTickEnabled(true);
	}

	bIsDualMode = true;
}

void ACSPlayerController::CloseDualMode()
{
	if (DualModeUI)
	{
		DualModeUI->RemoveFromParent();
	}

	if (CaptureP0)
	{
		CaptureP0->GetCaptureComponent2D()->SetComponentTickEnabled(false);
	}
	if (CaptureP1)
	{
		CaptureP1->GetCaptureComponent2D()->SetComponentTickEnabled(false);
	}

	bIsDualMode = false;
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