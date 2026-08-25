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
#include "SceneViewExtension.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Debug/DebugDrawService.h"
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
	// 분할 중 메인 사각형으로 좁혀 두었던 LocalPlayer 뷰포트를 전체로 되돌린다
	RestoreMainLocalPlayerViewport();
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

	// 보조 뷰가 아주 좁아지면 슬리버를 렌더하지 않고 바로 풀스크린으로 넘긴다.
	// (폭 16px 짜리 뷰는 종횡비가 극단적이라 화면이 뭉개지고, 전환 끝에서 툭 사라진다)
	const int32 MinViewWidth = FMath::Min(ViewRectAlignment * 2, ViewportSize.X);
	if (MainWidth > ViewportSize.X - MinViewWidth)
	{
		MainWidth = ViewportSize.X;
	}
	else if (MainWidth < MinViewWidth)
	{
		MainWidth = MinViewWidth;
	}

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

FSceneView* UCSViewFamilyViewportClient::AddSecondarySceneView(FSceneViewFamilyContext& ViewFamily, const FIntRect& ViewRect)
{
	UWorld* MyWorld = GetWorld();
	if (!MyWorld || ViewRect.Width() <= 0 || ViewRect.Height() <= 0)
	{
		return nullptr;
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
	return SecondarySceneView;
}

void UCSViewFamilyViewportClient::UpdateAudioListener(UWorld* ListenerWorld, APlayerController* PC, const FSceneView* View)
{
	// 엔진 Draw 는 매 프레임 리스너를 갱신한다. 분할 경로가 Super::Draw 를 우회하면서 이게 빠져
	// 분할이 켜진 순간의 위치에 3D 사운드가 얼어붙었다.
	if (!ListenerWorld || !PC || !View) return;

	FAudioDeviceHandle ListenerAudioDevice = ListenerWorld->GetAudioDevice();
	if (!ListenerAudioDevice.IsValid()) return;

	FVector Location;
	FVector ProjFront;
	FVector ProjRight;
	PC->GetAudioListenerPosition(Location, ProjFront, ProjRight);

	FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));
	ListenerTransform.SetTranslation(Location);
	ListenerTransform.NormalizeRotation();

	const uint32 ViewportIndex = 0;
	ListenerAudioDevice->SetListener(ListenerWorld, ViewportIndex, ListenerTransform, View->bCameraCut ? 0.f : ListenerWorld->GetDeltaSeconds());

	FVector OverrideAttenuation;
	if (PC->GetAudioListenerAttenuationOverridePosition(OverrideAttenuation))
	{
		ListenerAudioDevice->SetListenerAttenuationOverride(ViewportIndex, OverrideAttenuation);
	}
	else
	{
		ListenerAudioDevice->ClearListenerAttenuationOverride(ViewportIndex);
	}
}

void UCSViewFamilyViewportClient::RestoreMainLocalPlayerViewport()
{
	// 분할 중에는 Origin/Size 를 메인 사각형으로 유지한다 (투영/디프로젝션 API 가 실제 렌더 영역과 맞도록).
	// 분할이 끝나면 전체 뷰포트로 되돌린다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULocalPlayer* LP = GI->GetFirstGamePlayer())
		{
			LP->Origin = FVector2D::ZeroVector;
			LP->Size = FVector2D(1.f, 1.f);
		}
	}
}

void UCSViewFamilyViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	TickFade(GetWorld() ? GetWorld()->DeltaTimeSeconds : 0.016f);

	UWorld* MyWorld = GetWorld();
	UGameInstance* GI = GetGameInstance();
	ULocalPlayer* MainLocalPlayer = GI ? GI->GetFirstGamePlayer() : nullptr;
	const FIntPoint ViewportSize = InViewport ? InViewport->GetSizeXY() : FIntPoint::ZeroValue;

	// 보조 뷰가 비활성이거나 전제가 깨지면 엔진 기본 Draw 로 폴백 (싱글 뷰 / 메뉴 / 로비)
	const bool bCanDrawSplit =
		bSecondaryActive && MyWorld && GI && InViewport && SceneCanvas &&
		MainLocalPlayer && MainLocalPlayer->PlayerController && MyWorld->Scene &&
		ViewportSize.X > 0 && ViewportSize.Y > 0;

	if (!bCanDrawSplit)
	{
		RestoreMainLocalPlayerViewport();
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	FIntRect MainRect, SecondaryRect;
	ComputeViewRects(ViewportSize, MainRect, SecondaryRect);

	// (removed 2026-08-25: ViewRect 가 바뀐 프레임에 SetGameCameraCutThisFrame() 을 걸던 코드.
	//  전환 중 경계는 프레임당 ~32px 씩 움직여 8px 스냅으로도 매 프레임 바뀌므로, 결과적으로
	//  전환 내내 매 프레임 카메라 컷이 걸렸다 - 오토 노출이 매 프레임 리셋돼 화면이 깜빡이고
	//  TSR 은 누적을 못 했다. 이음새 노이즈의 실제 원인은 r.ScreenPercentage=71 로 확인됐다.)

	APlayerController* MainPC = MainLocalPlayer->PlayerController;

	// ===== ViewFamily 빌드 =====
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewport, MyWorld->Scene, EngineShowFlags)
		.SetRealtimeUpdate(true));

	ViewFamily.DebugDPIScale = GetDPIScale();
	ViewFamily.EngineShowFlags = EngineShowFlags;
	// 콘솔 viewmode(wireframe / unlit 등)를 따르도록 - 예전엔 VMI_Lit 하드코딩이라 뷰모드가 먹지 않았다
	ViewFamily.ViewMode = EViewModeIndex(ViewModeIndex);
	ViewFamily.bIsMainViewFamily = true;

	ViewFamily.Time = FGameTime::CreateUndilated(MyWorld->TimeSeconds, MyWorld->DeltaTimeSeconds);

	// ===== SceneViewExtension 수집 =====
	// FSR/DLSS/OCIO 같은 확장은 여기서 등록되지 않으면 분할 중 조용히 꺼진다.
	if (GEngine && GEngine->ViewExtensions.IsValid())
	{
		FSceneViewExtensionContext ViewExtensionContext(InViewport);
		ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(ViewExtensionContext);
		for (const FSceneViewExtensionRef& ViewExt : ViewFamily.ViewExtensions)
		{
			ViewExt->SetupViewFamily(ViewFamily);
		}
	}

	// ===== 메인 뷰 (LocalPlayer 사용) =====
	// LocalPlayer->Origin/Size 는 CalcSceneView 안에서 ViewRect 와 종횡비 산출에 쓰이고,
	// ProjectWorldLocationToScreen / DeprojectMousePosition 등도 같은 값을 읽는다.
	// 분할 중에는 계속 메인 사각형으로 유지해야 화면 좌표 변환이 실제 렌더 영역과 맞는다.
	{
		const float InvW = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.X));
		const float InvH = 1.f / FMath::Max(1.f, static_cast<float>(ViewportSize.Y));

		MainLocalPlayer->Origin = FVector2D(static_cast<float>(MainRect.Min.X) * InvW,
		                                    static_cast<float>(MainRect.Min.Y) * InvH);
		MainLocalPlayer->Size = FVector2D(static_cast<float>(MainRect.Width()) * InvW,
		                                  static_cast<float>(MainRect.Height()) * InvH);
	}

	FVector OutViewLocation = FVector::ZeroVector;
	FRotator OutViewRotation = FRotator::ZeroRotator;
	FSceneView* MainSceneView = MainLocalPlayer->CalcSceneView(&ViewFamily, OutViewLocation, OutViewRotation, InViewport);

	if (!MainSceneView)
	{
		// 투영 데이터 산출 실패 (PC 교체 직후 등). 뷰 0개로 렌더러에 넘기면 크래시/검은 화면.
		RestoreMainLocalPlayerViewport();
		Super::Draw(InViewport, SceneCanvas);
		DrawFadeOverlay(InViewport);
		return;
	}

	// 엔진 Draw 가 뷰마다 하던 갱신들
	MainSceneView->CameraConstrainedViewRect = MainSceneView->UnscaledViewRect;
	MainLocalPlayer->LastViewLocation = OutViewLocation;
	AddStreamingViewInfo(*MyWorld, *MainSceneView);	// 텍스처 스트리밍 뷰 정보 (없으면 밉이 안 올라온다)
	MyWorld->LastRenderTime = MyWorld->GetTimeSeconds();
	UpdateAudioListener(MyWorld, MainPC, MainSceneView);

	// ===== 보조 뷰 (LocalPlayer 없이 직접 빌드) =====
	if (SecondaryRect.Width() > 0 && SecondaryRect.Height() > 0)
	{
		if (FSceneView* SecondarySceneView = AddSecondarySceneView(ViewFamily, SecondaryRect))
		{
			// CalcSceneView 가 메인 뷰에 대해 해 주는 일을 보조 뷰에는 직접 해 준다
			for (const FSceneViewExtensionRef& ViewExt : ViewFamily.ViewExtensions)
			{
				ViewExt->SetupView(ViewFamily, *SecondarySceneView);
			}

			SecondarySceneView->CameraConstrainedViewRect = SecondarySceneView->UnscaledViewRect;
			AddStreamingViewInfo(*MyWorld, *SecondarySceneView);
		}
	}

	// ===== 카메라 컷 플래그 정리 =====
	// 엔진 Draw 가 매 프레임 해 주던 일. 안 하면 SetGameCameraCutThisFrame() 이 영구히 true 로 남아
	// TSR 이 계속 리셋되고 오디오 리스너 보간도 죽는다.
	bool bAnyPlayerCameraCut = false;
	if (APlayerCameraManager* CamMgr = MainPC->PlayerCameraManager)
	{
		bAnyPlayerCameraCut = CamMgr->bGameCameraCutThisFrame;
		CamMgr->bGameCameraCutThisFrame = false;
	}
	InViewport->SetCameraCut(bAnyPlayerCameraCut);

	// ===== FinalizeViews =====
	{
		TMap<ULocalPlayer*, FSceneView*> PlayerViewMap;
		PlayerViewMap.Add(MainLocalPlayer, MainSceneView);
		FinalizeViews(&ViewFamily, PlayerViewMap);
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

	if (!bDisableWorldRendering && ViewFamily.Views.Num() > 0)
	{
		IRendererModule& RendererModule = FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
		RendererModule.BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
	}

	// 스크린샷 / 무비 캡처 (엔진 Draw 가 하던 단계)
	ProcessScreenShots(InViewport);

	// ===== PostRender 단계 (HUD / Console / OnScreenDebug 등) =====
	// HUD 는 엔진과 동일하게 SceneCanvas 에 메인 뷰 사각형 기준으로 그린다.
	// Canvas 에 SceneView 를 넣어야 Canvas Project / AHUD Project 가 동작한다 (없으면 전부 0 이 나온다).
	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();

	if (!SceneCanvasObject)
	{
		SceneCanvasObject = NewObject<UCanvas>(GetTransientPackage());
	}
	if (!DebugCanvasObject)
	{
		DebugCanvasObject = NewObject<UCanvas>(GetTransientPackage());
	}

	if (DebugCanvas)
	{
		DebugCanvasObject->Init(ViewportSize.X, ViewportSize.Y, MainSceneView, DebugCanvas);
	}

	{
		const FVector CanvasOrigin(FMath::TruncToFloat(static_cast<float>(MainSceneView->UnscaledViewRect.Min.X)),
		                           FMath::TruncToFloat(static_cast<float>(MainSceneView->UnscaledViewRect.Min.Y)), 0.f);

		SceneCanvasObject->Init(MainSceneView->UnscaledViewRect.Width(), MainSceneView->UnscaledViewRect.Height(), MainSceneView, SceneCanvas);

		SceneCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
		SceneCanvasObject->ApplySafeZoneTransform();

		// 1) HUD - 메인 LocalPlayer 의 Canvas HUD (UMG 는 Slate 가 별도 처리)
		if (AHUD* HUD = MainPC->GetHUD())
		{
			DebugCanvasObject->SceneView = MainSceneView;
			HUD->SetCanvas(SceneCanvasObject, DebugCanvasObject);
			HUD->PostRender();

			// PostRender 중 BP 브레이크포인트 등으로 포인터가 바뀔 수 있어 되돌린다
			SceneCanvasObject->Canvas = SceneCanvas;
			DebugCanvasObject->Canvas = DebugCanvas;

			if (IsValid(MainPC))
			{
				HUD->SetCanvas(nullptr, nullptr);
			}
		}

		// 2) 디버그 드로잉 (DrawDebug 계열 / ShowFlag 기반)
		if (DebugCanvas)
		{
			DebugCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
			UDebugDrawService::Draw(ViewFamily.EngineShowFlags, InViewport, MainSceneView, DebugCanvas, DebugCanvasObject, MainPC);
			DebugCanvas->PopTransform();
		}

		SceneCanvasObject->PopSafeZoneTransform();
		SceneCanvas->PopTransform();
	}

	if (DebugCanvas)
	{
		DebugCanvasObject->ApplySafeZoneTransform();

		// 3) PostRender - transition / title-safe
		PostRender(DebugCanvasObject);

		// 4) 콘솔 입력창
		if (ViewportConsole)
		{
			ViewportConsole->PostRender_Console(DebugCanvasObject);
		}

		// 5) OnScreenDebugMessage
		if (GEngine)
		{
			GEngine->DrawOnscreenDebugMessages(MyWorld, InViewport, DebugCanvas, DebugCanvasObject, 40.0f, 100.0f);
		}

		// 6) Stats - stat fps / stat unit / stat game 등
		{
			FVector PlayerCameraLocation = FVector::ZeroVector;
			FRotator PlayerCameraRotation = FRotator::ZeroRotator;
			MainPC->GetPlayerViewPoint(PlayerCameraLocation, PlayerCameraRotation);

			DrawStatsHUD(MyWorld, InViewport, DebugCanvas, DebugCanvasObject,
			             DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
		}

		DebugCanvasObject->PopSafeZoneTransform();
	}

	DrawFadeOverlay(InViewport);
}
