// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "ChronoSpace.h"
#include "UI/SCSServerTravelWidget.h"
#include "UI/CSGameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Character/CSCharacterPlayer.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"


ACSPlayerController::ACSPlayerController()
{
	// MouseCursor 
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	check(CameraShake);

	SetupInputMode();

	// UI ������ �ణ ���� (PlayerState �ʱ�ȭ ���)
	GetWorldTimerManager().SetTimer(UICreationTimerHandle, this, &ACSPlayerController::InitializeUI, 0.2f, false);

	if ( IsLocalController() )
	{
		int32 Width = 960;
		int32 Height = 1080;

		FVector2D ScreenResolution;

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ScreenResolution);
			Width = FMath::Max( Width, ScreenResolution.X / 2 );
			Height = FMath::Max( Height, ScreenResolution.Y / 2 );
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

	// Pawn�� ����� �� UI ���ΰ�ħ
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState�� ������ �� UI ���ΰ�ħ
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ÿ�̸� ����
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);

	// UI ����
	if (GameUIWidget)
	{
		GameUIWidget->RemoveFromParent();
		GameUIWidget = nullptr;
	}

	// Slate UI ����
	if (ServerTravelWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ServerTravelWidget.ToSharedRef());
		ServerTravelWidget.Reset();
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
	// ���� ���� �Է� ��� ����
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void ACSPlayerController::InitializeUI()
{
	// ���� �÷��̾� ��Ʈ�ѷ������� UI ����
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
	ACSCharacterPlayer* LeftCharacter;
	ACSCharacterPlayer* RightCharacter;

	if ( HasAuthority() )	// Server
	{
		LeftCharacter = Cast<ACSCharacterPlayer>(GetPawn());
		RightCharacter = FindFirstOtherPawn();
	}
	else					// Client
	{
		LeftCharacter = FindFirstOtherPawn();
		RightCharacter = Cast<ACSCharacterPlayer>(GetPawn());
	}

	if (!LeftCharacter) return;

	ASceneCapture2D* CapLeft = SpawnCaptureAndAttach(LeftCharacter->GetFollowCamera(), RenderTargetP0);

	if ( CapLeft )
	{
		UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UpdateRenderTarget - CapLeft Success"), GetWorld()->GetNetMode());
	}

	if (!RightCharacter) return;

	ASceneCapture2D* CapRight = SpawnCaptureAndAttach(RightCharacter->GetFollowCamera(), RenderTargetP1);

	if (CapRight)
	{
		UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UpdateRenderTarget - CapRight Success"), GetWorld()->GetNetMode());
	}
}

ACSCharacterPlayer* ACSPlayerController::FindFirstOtherPawn()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return nullptr;

	for( TActorIterator<ACSCharacterPlayer> It(GetWorld()) ; It; ++It)
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

	if (!CaptureActor) return nullptr;

	USceneCaptureComponent2D* CaptureComp = CaptureActor->GetCaptureComponent2D();
	CaptureComp->TextureTarget = TargetRT;
	CaptureComp->CaptureSource = SCS_FinalColorLDR;
	CaptureComp->bCaptureEveryFrame = true;

	CaptureActor->AttachToComponent( Cast<USceneComponent>(TargetCam), FAttachmentTransformRules::SnapToTargetIncludingScale);

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

	if ( !DualModeUI )
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
	if ( DualModeUI )
	{
		DualModeUI->RemoveFromParent();
	}

	if ( CaptureP0 )
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
	// ���� �÷��̾���� UI ǥ��
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

		// UI ���ΰ�ħ
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
	// ���� �÷��̾���� ����
	if (!IsLocalPlayerController())
	{
		return;
	}

	// �̹� �����Ǿ� ������ �н�
	if (ServerTravelWidget.IsValid())
	{
		return;
	}

	// Slate ���� ����
	ServerTravelWidget = SNew(SCSServerTravelWidget);

	if (GEngine && ServerTravelWidget.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			TEXT("Server Travel Widget Created"));
	}
}

void ACSPlayerController::ShowServerTravelUI()
{
	// ���� �÷��̾���� ����
	if (!IsLocalPlayerController())
	{
		return;
	}

	// ������ ������ ����
	CreateServerTravelWidget();

	// GameViewport�� �߰�
	if (ServerTravelWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			ServerTravelWidget.ToSharedRef(),
			1000  // Z-Order (�������� �տ� ǥ��)
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
	// Slate ������ ���ü��� GameViewport�� �߰��Ǿ� �ִ����� �Ǵ�
	// ��Ȯ�� üũ�� ���ؼ��� ������ bool ������ �����ϴ� ���� ����
	return ServerTravelWidget.IsValid();
}