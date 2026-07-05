// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/CSViewFamilyViewportClient.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"
#include "RendererInterface.h"
#include "Modules/ModuleManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Console.h"
#include "LegacyScreenPercentageDriver.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Math/InverseRotationMatrix.h"
#include "UObject/UObjectGlobals.h"
#include "ChronoSpace.h"

UCSViewFamilyViewportClient::UCSViewFamilyViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCSViewFamilyViewportClient::OnPostLoadMap);
}

void UCSViewFamilyViewportClient::BeginDestroy()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	Super::BeginDestroy();
}

void UCSViewFamilyViewportClient::StartFade(float TargetAlpha, float Duration)
{
	FadeTargetAlpha = FMath::Clamp(TargetAlpha, 0.f, 1.f);
	FadeSpeed = (Duration > KINDA_SMALL_NUMBER) ? (1.f / Duration) : 1000.f;
}

void UCSViewFamilyViewportClient::TickFade(float DeltaSeconds)
{
	if (!FMath::IsNearlyEqual(FadeAlpha, FadeTargetAlpha))
	{
		FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTargetAlpha, DeltaSeconds, FadeSpeed);
	}
}

void UCSViewFamilyViewportClient::DrawFadeOverlay(FViewport* InViewport)
{
	if (!InViewport || FadeAlpha <= KINDA_SMALL_NUMBER) return;

	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();
	if (!DebugCanvas) return;

	const FIntPoint Size = InViewport->GetSizeXY();
	FCanvasTileItem Tile(FVector2D::ZeroVector, FVector2D(Size), FLinearColor(0.f, 0.f, 0.f, FadeAlpha));
	Tile.BlendMode = SE_BLEND_Translucent;
	DebugCanvas->DrawItem(Tile);
}

void UCSViewFamilyViewportClient::OnPostLoadMap(UWorld* /*LoadedWorld*/)
{
	FadeAlpha = 0.f;
	FadeTargetAlpha = 0.f;
}

void UCSViewFamilyViewportClient::SetSecondaryView(const FVector& InLocation, const FRotator& InRotation, float InFOV)
{
	SecondaryLocation = InLocation;
	SecondaryRotation = InRotation;
	SecondaryFOV = (InFOV > KINDA_SMALL_NUMBER) ? InFOV : 90.f;
	bSecondaryActive = true;
}

void UCSViewFamilyViewportClient::ClearSecondaryView()
{
	bSecondaryActive = false;
}

void UCSViewFamilyViewportClient::ComputeViewRects(const FIntPoint& ViewportSize, FIntRect& OutMainRect, FIntRect& OutSecondaryRect) const
{
	const float SmoothedAlpha = FMath::InterpEaseInOut(0.f, 1.f, FullscreenAlpha, 2.f);

	const int32 HalfX = FMath::Max(1, ViewportSize.X / 2);
	const int32 MainWidth = FMath::Clamp(
		static_cast<int32>(FMath::Lerp(static_cast<float>(HalfX), static_cast<float>(ViewportSize.X), SmoothedAlpha)),
		1, ViewportSize.X);

	if (!bSwapLeftRight)
	{
		OutMainRect = FIntRect(0, 0, MainWidth, ViewportSize.Y);
		OutSecondaryRect = FIntRect(MainWidth, 0, ViewportSize.X, ViewportSize.Y);
	}
	else
	{
		const int32 SecondaryWidth = FMath::Max(0, ViewportSize.X - MainWidth);
		OutSecondaryRect = FIntRect(0, 0, SecondaryWidth, ViewportSize.Y);
		OutMainRect = FIntRect(SecondaryWidth, 0, ViewportSize.X, ViewportSize.Y);
	}
}

void UCSViewFamilyViewportClient::AddSecondarySceneView(FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect)
{
	UWorld* MyWorld = GetWorld();
	if (!MyWorld || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
	{
		return;
	}

	if (!SecondaryViewState.GetReference())
	{
		SecondaryViewState.Allocate(MyWorld->GetFeatureLevel());
	}

	FSceneViewInitOptions ViewInit;
	ViewInit.ViewFamily = &ViewFamily;
	ViewInit.SceneViewStateInterface = SecondaryViewState.GetReference();
	ViewInit.ViewActor = nullptr;
	ViewInit.PlayerIndex = INDEX_NONE;
	ViewInit.SetViewRectangle(ViewRect);
	ViewInit.BackgroundColor = FLinearColor::Black;
	ViewInit.OverlayColor = FLinearColor::Transparent;
	ViewInit.FOV = SecondaryFOV;
	ViewInit.DesiredFOV = SecondaryFOV;

	// View 행렬 (UE 표준 — ULocalPlayer::GetProjectionData 와 동일 패턴)
	ViewInit.ViewOrigin = SecondaryLocation;
	ViewInit.ViewRotationMatrix = FInverseRotationMatrix(SecondaryRotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// 투영 행렬 — ULocalPlayer 가 메인 뷰에 적용하는 AspectRatioAxisConstraint 와 동일한 보정 적용.
	// 기본값 (MaintainYFOV) 일 때, 화면 종횡비가 좁아지면 (분할 화면) 가로 FOV 를 줄여 세로 FOV 를 유지.
	// 이 보정을 안 하면 보조 뷰는 90° 와이드 그대로 → 메인보다 화각이 넓어져 캐릭터가 작게 보임.
	{
		const float NewAspect = static_cast<float>(ViewRect.Width()) / FMath::Max(1.f, static_cast<float>(ViewRect.Height()));

		// LocalPlayer 의 AspectRatioAxisConstraint 조회 (기본: MaintainYFOV 또는 MajorAxisFOV)
		EAspectRatioAxisConstraint AspectConstraint = AspectRatio_MajorAxisFOV;
		if (UGameInstance* GIRef = GetGameInstance())
		{
			if (ULocalPlayer* LP = GIRef->GetFirstGamePlayer())
			{
				AspectConstraint = LP->AspectRatioAxisConstraint;
			}
		}

		// FOV 가 정의된 기준 종횡비 (UE 표준: 16:9). 이 종횡비에서 SecondaryFOV 가 가로 FOV.
		constexpr float ReferenceAspect = 16.f / 9.f;

		// 새 종횡비에서 사용할 가로 FOV 계산
		float MatrixFOV_Deg = SecondaryFOV;
		const bool bMaintainYFOV =
			(AspectConstraint == AspectRatio_MaintainYFOV) ||
			(AspectConstraint == AspectRatio_MajorAxisFOV && NewAspect < ReferenceAspect);
		if (bMaintainYFOV)
		{
			// 기준 종횡비에서의 세로 FOV → 새 종횡비에서의 가로 FOV
			const float OrigHFOV_Rad = FMath::DegreesToRadians(SecondaryFOV);
			const float YFOV_Rad = 2.f * FMath::Atan(FMath::Tan(OrigHFOV_Rad * 0.5f) / ReferenceAspect);
			const float NewHFOV_Rad = 2.f * FMath::Atan(FMath::Tan(YFOV_Rad * 0.5f) * NewAspect);
			MatrixFOV_Deg = FMath::RadiansToDegrees(NewHFOV_Rad);
		}

		FMinimalViewInfo MinView;
		MinView.Location = SecondaryLocation;
		MinView.Rotation = SecondaryRotation;
		MinView.FOV = MatrixFOV_Deg;
		MinView.DesiredFOV = MatrixFOV_Deg;
		MinView.AspectRatio = NewAspect;
		MinView.bConstrainAspectRatio = false;
		MinView.ProjectionMode = ECameraProjectionMode::Perspective;
		ViewInit.ProjectionMatrix = MinView.CalculateProjectionMatrix();
	}

	FSceneView* SecondarySceneView = new FSceneView(ViewInit);
	SecondarySceneView->ViewLocation = SecondaryLocation;
	SecondarySceneView->ViewRotation = SecondaryRotation;

	// 메인 ViewFamily 의 EngineShowFlags 를 그대로 따라감
	SecondarySceneView->StartFinalPostprocessSettings(SecondaryLocation);
	SecondarySceneView->EndFinalPostprocessSettings(ViewInit);

	ViewFamily.Views.Add(SecondarySceneView);
}

void UCSViewFamilyViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	TickFade(GetWorld() ? GetWorld()->DeltaTimeSeconds : 0.016f);

	// 보조 뷰가 비활성이면 엔진 기본 Draw 사용 (싱글 뷰 / 메뉴 / 로비)
	if (!bSecondaryActive)
	{
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	UWorld* MyWorld = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!MyWorld || !GI || !InViewport || !SceneCanvas)
	{
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	ULocalPlayer* MainLocalPlayer = GI->GetFirstGamePlayer();
	if (!MainLocalPlayer || !MainLocalPlayer->PlayerController || !MyWorld->Scene)
	{
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	const FIntPoint ViewportSize = InViewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	FIntRect MainRect, SecondaryRect;
	ComputeViewRects(ViewportSize, MainRect, SecondaryRect);

	// ===== ViewFamily 빌드 =====
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport, MyWorld->Scene, EngineShowFlags)
		.SetRealtimeUpdate(true));

	ViewFamily.DebugDPIScale = GetDPIScale();
	ViewFamily.EngineShowFlags = EngineShowFlags;
	ViewFamily.ViewMode = VMI_Lit;

	ViewFamily.Time = FGameTime::CreateUndilated(MyWorld->TimeSeconds, MyWorld->DeltaTimeSeconds);

	// ===== 메인 뷰 (LocalPlayer 사용) =====
	// LocalPlayer->Origin/Size 가 CalcSceneView 안에서 ViewRect 와 ProjectionMatrix 의 AspectRatio 산출에
	// 사용된다. 분할 사각형에 맞춰 잠시 덮어썼다가 호출 후 복원한다.
	{
		const FVector2D SavedOrigin = MainLocalPlayer->Origin;
		const FVector2D SavedSize = MainLocalPlayer->Size;

		const float InvW = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.X));
		const float InvH = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.Y));

		MainLocalPlayer->Origin = FVector2D(static_cast<float>(MainRect.Min.X) * InvW,
		                                    static_cast<float>(MainRect.Min.Y) * InvH);
		MainLocalPlayer->Size = FVector2D(static_cast<float>(MainRect.Width()) * InvW,
		                                  static_cast<float>(MainRect.Height()) * InvH);

		FVector OutViewLocation;
		FRotator OutViewRotation;
		MainLocalPlayer->CalcSceneView(&ViewFamily, OutViewLocation, OutViewRotation, InViewport);

		MainLocalPlayer->Origin = SavedOrigin;
		MainLocalPlayer->Size = SavedSize;
	}

	// ===== 보조 뷰 (LocalPlayer 없이 직접 빌드) =====
	if (SecondaryRect.Width() > 0 && SecondaryRect.Height() > 0)
	{
		AddSecondarySceneView(ViewFamily, SecondaryRect);
	}

	// ===== ScreenPercentage 인터페이스 등록 =====
	// UGameViewportClient::Draw 가 평소 자동으로 처리하던 단계.
	// FSceneViewFamily 는 SetScreenPercentageInterface 로 받은 new 객체의 소유권을 가져가 소멸 시 해제한다.
	if (!ViewFamily.GetScreenPercentageInterface())
	{
		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			ViewFamily,
			FLegacyScreenPercentageDriver::GetCVarResolutionFraction()));
	}

	// ===== 렌더 큐잉 =====
	SceneCanvas->Clear(FLinearColor::Black);
	IRendererModule& RendererModule = FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
	RendererModule.BeginRenderingViewFamily(SceneCanvas, &ViewFamily);

	// ===== PostRender 단계 (HUD / Console / OnScreenDebug 등) =====
	// 엔진 표준 Draw() 가 BeginRenderingViewFamily 직후에 처리하던 일들을 직접 수행.
	// PostRender(UCanvas*) 가상 메서드는 *transition / title-safe* 만 그리므로
	// 콘솔 / HUD / OnScreenDebug 는 별도 호출이 필요하다.
	if (FCanvas* DebugCanvas = InViewport->GetDebugCanvas())
	{
		if (!DebugCanvasObject)
		{
			DebugCanvasObject = NewObject<UCanvas>(GetTransientPackage());
		}

		DebugCanvasObject->Init(ViewportSize.X, ViewportSize.Y, nullptr, DebugCanvas);
		DebugCanvasObject->ApplySafeZoneTransform();

		// 1) HUD — 메인 LocalPlayer 의 Canvas HUD 만 그린다 (UMG 는 Slate 가 별도 처리)
		if (APlayerController* PC = MainLocalPlayer->PlayerController)
		{
			if (AHUD* HUD = PC->GetHUD())
			{
				HUD->SetCanvas(DebugCanvasObject, DebugCanvasObject);
				HUD->PostRender();
				HUD->SetCanvas(nullptr, nullptr);
			}
		}

		// 2) PostRender — transition / title-safe
		PostRender(DebugCanvasObject);

		// 3) 콘솔 (`키 입력 후 화면에 보이는 입력창)
		if (ViewportConsole)
		{
			ViewportConsole->PostRender_Console(DebugCanvasObject);
		}

		// 4) OnScreenDebugMessage — AddOnScreenDebugMessage / 디버그 그리기
		if (GEngine)
		{
			GEngine->DrawOnscreenDebugMessages(MyWorld, InViewport, SceneCanvas, DebugCanvasObject, 40.0f, 100.0f);
		}

		// 5) Stats — stat fps / stat unit / stat game 등 stat 시스템 출력
		{
			FVector PlayerCameraLocation = FVector::ZeroVector;
			FRotator PlayerCameraRotation = FRotator::ZeroRotator;
			if (APlayerController* PC = MainLocalPlayer->PlayerController)
			{
				PC->GetPlayerViewPoint(PlayerCameraLocation, PlayerCameraRotation);
			}
			DrawStatsHUD(MyWorld, InViewport, DebugCanvas, DebugCanvasObject,
			             DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
		}

		DebugCanvasObject->PopSafeZoneTransform();
	}

	DrawFadeOverlay(InViewport);
}
