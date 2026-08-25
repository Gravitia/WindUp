// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/CSViewFamilyViewportClient.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"
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

void UCSViewFamilyViewportClient::SetSecondaryView(const FVector& InLocation, const FRotator& InRotation, float InFOV, float InAspectRatio)
{
	SecondaryLocation = InLocation;
	SecondaryRotation = InRotation;
	SecondaryFOV = (InFOV > KINDA_SMALL_NUMBER) ? InFOV : 90.f;
	SecondaryAspectRatio = (InAspectRatio > KINDA_SMALL_NUMBER) ? InAspectRatio : (16.f / 9.f);
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
	int32 MainWidth = static_cast<int32>(FMath::Lerp(static_cast<float>(HalfX), static_cast<float>(ViewportSize.X), SmoothedAlpha));

	// 경계를 ViewRectAlignment 픽셀 배수로 스냅한다.
	// TSR/업스케일은 타일 단위로 동작하고 ScreenPercentage 가 100% 가 아니면 각 뷰의 사각형이
	// 따로 스케일·반올림되는데, 경계가 정렬돼 있지 않으면 두 뷰 사이에 아무도 쓰지 않는
	// 1~2 픽셀 열이 생겨 화면 중앙에 노이즈(깨진 화소)로 보인다.
	if (ViewRectAlignment > 1 && MainWidth < ViewportSize.X)
	{
		MainWidth = (MainWidth / ViewRectAlignment) * ViewRectAlignment;
	}

	// 두 뷰가 모두 보이는 동안에는 각 뷰가 최소 폭을 갖도록 (0 폭 뷰 / 극단적 종횡비 방지)
	const int32 MinViewWidth = FMath::Min(ViewRectAlignment * 2, ViewportSize.X);
	MainWidth = (SmoothedAlpha >= 1.f - KINDA_SMALL_NUMBER)
		? ViewportSize.X
		: FMath::Clamp(MainWidth, MinViewWidth, ViewportSize.X - MinViewWidth);

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

void UCSViewFamilyViewportClient::AddSecondarySceneView(FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect, bool bCameraCut)
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
	// 경계가 움직인 프레임에는 히스토리를 버려 이음새 노이즈를 막는다
	ViewInit.bInCameraCut = bCameraCut;

	// View 행렬 (UE 표준 — ULocalPlayer::GetProjectionData 와 동일 패턴)
	ViewInit.ViewOrigin = SecondaryLocation;
	ViewInit.ViewRotationMatrix = FInverseRotationMatrix(SecondaryRotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// 투영 행렬 — 메인 뷰와 "같은 함수"를 쓴다.
	// ULocalPlayer::GetProjectionData 도 결국 CalculateProjectionMatrixGivenViewRectangle 을 호출한다.
	// 예전엔 16:9 기준 FOV 를 손으로 변환한 뒤 FMinimalViewInfo::CalculateProjectionMatrix() 에 넘겼는데,
	// 그 함수는 AspectRatio 를 한 번 더 적용하는 다른 공식이라(FReversedZPerspectiveMatrix(HalfFOV, AspectRatio, 1, ...))
	// 종횡비 보정이 이중으로 걸려 보조 뷰의 화각이 메인 뷰와 달라졌다.
	{
		EAspectRatioAxisConstraint AspectConstraint = AspectRatio_MajorAxisFOV;
		if (UGameInstance* GIRef = GetGameInstance())
		{
			if (ULocalPlayer* LP = GIRef->GetFirstGamePlayer())
			{
				AspectConstraint = LP->AspectRatioAxisConstraint;
			}
		}

		FMinimalViewInfo MinView;
		MinView.Location = SecondaryLocation;
		MinView.Rotation = SecondaryRotation;
		MinView.FOV = SecondaryFOV;
		MinView.DesiredFOV = SecondaryFOV;
		// FOV 가 정의된 기준 종횡비 (카메라 컴포넌트의 AspectRatio, 기본 16:9)
		MinView.AspectRatio = SecondaryAspectRatio;
		MinView.bConstrainAspectRatio = false;
		MinView.ProjectionMode = ECameraProjectionMode::Perspective;

		FSceneViewProjectionData ProjData;
		ProjData.ViewOrigin = SecondaryLocation;
		ProjData.ViewRotationMatrix = ViewInit.ViewRotationMatrix;
		ProjData.SetViewRectangle(ViewRect);

		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(MinView, AspectConstraint, ViewRect, ProjData);
		ViewInit.ProjectionMatrix = ProjData.ProjectionMatrix;
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

	// 분할 경계가 움직인 프레임에는 임시 히스토리(TSR/모션블러)를 버린다.
	// ViewRect 가 바뀌면 TSR 이 지난 프레임 히스토리를 다른 영역에서 리샘플하게 되고,
	// 새로 드러난 경계 부근 픽셀에는 유효한 히스토리가 없어 색이 튀는 깨진 화소가 나타난다.
	// (경계를 8px 로 스냅했으므로 전환 중에도 실제로 바뀌는 프레임은 몇 프레임뿐이다)
	const bool bViewRectChanged = (LastMainRectWidth != INDEX_NONE) && (LastMainRectWidth != MainRect.Width());
	LastMainRectWidth = MainRect.Width();

	if (bViewRectChanged)
	{
		if (APlayerController* PC = MainLocalPlayer->PlayerController)
		{
			if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				// CalcSceneView 가 이 값을 읽어 메인 뷰에 카메라 컷을 적용한다
				CamMgr->SetGameCameraCutThisFrame();
			}
		}
	}

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
		AddSecondarySceneView(ViewFamily, SecondaryRect, bViewRectChanged);
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
