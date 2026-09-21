// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSCameraOcclusionFadeSubsystem.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "Character/CSCharacterPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ChronoSpace.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialCachedData.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/IConsoleManager.h"
#endif

namespace CSCameraOcclusionFade
{
	// 캡슐 위 샘플 격자: 세로 4단 × 가로 3열 = 12 광선
	static const float RowFractions[] = { -0.6f, -0.2f, 0.2f, 0.6f };
	static const float ColFractions[] = { -0.7f, 0.f, 0.7f };

	// cs.CameraFade.Debug 1 : 매 틱 후보·페이드 상태를 화면에 찍고, 판정 광선을 그린다
	static TAutoConsoleVariable<int32> CVarDebug(
		TEXT("cs.CameraFade.Debug"), 0,
		TEXT("카메라 가림 페이드 디버그. 1 이면 후보 수, 페이드 중인 액터와 값, 샘플 광선을 화면에 표시한다."),
		ECVF_Cheat);
}

bool UCSCameraOcclusionFadeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;

	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UCSCameraOcclusionFadeSubsystem::Deinitialize()
{
	RestoreAll();
	Targets.Reset();
	Super::Deinitialize();
}

TStatId UCSCameraOcclusionFadeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCSCameraOcclusionFadeSubsystem, STATGROUP_Tickables);
}

// ─────────────────────────────────────────────────────────────────────────────
// 등록
// ─────────────────────────────────────────────────────────────────────────────

void UCSCameraOcclusionFadeSubsystem::RegisterTarget(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	Targets.AddUnique(Actor);
	ValidateActorMaterials(Actor);
}

void UCSCameraOcclusionFadeSubsystem::UnregisterTarget(AActor* Actor)
{
	if (!Actor) return;

	Targets.Remove(Actor);

	if (const FCSCameraOcclusionFadeEntry* Entry = Faded.Find(Actor))
	{
		RestoreEntry(*Entry);
		Faded.Remove(Actor);
	}
}

bool UCSCameraOcclusionFadeSubsystem::IsMaterialFadeReady(const UMaterialInterface* Material)
{
	if (!Material) return true;	// 빈 슬롯은 아무것도 그리지 않는다
	if (Material->GetBlendMode() != BLEND_Masked) return false;

	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const UMaterialFunctionInterface* Fn = Settings ? Settings->FadeFunction.LoadSynchronous() : nullptr;
	if (!Fn) return true;	// 검사할 기준이 없으면 검사하지 않는다

	// 그래프(에디터 전용 데이터) 대신 캐시된 함수 목록을 본다. 중첩 호출까지 들어 있고 쿠킹된 빌드에도 남는다.
	for (const FMaterialFunctionInfo& Info : Material->GetCachedExpressionData().FunctionInfos)
	{
		if (Info.Function == Fn) return true;
	}
	return false;
}

#if WITH_EDITOR
namespace
{
	/**
	 * 설정의 PrimitiveDataIndex 와 MF_CameraFade 안 파라미터의 PrimitiveDataIndex 가 같은지 확인한다. 설정값이 바뀔 때마다 다시 본다.
	 * 둘은 따로 저장되는 값이라 어긋나도 컴파일은 통과하고, 그저 페이드가 조용히 안 될 뿐이다.
	 */
	void ValidateFadeFunctionIndex()
	{
		const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
		if (!Settings) return;

		// 설정값이 바뀌면 다시 검사한다. 세션당 한 번만 하면 "작업 중 인덱스를 바꿨을 때" 정작 침묵한다.
		static int32 LastCheckedIndex = INDEX_NONE;
		if (LastCheckedIndex == Settings->PrimitiveDataIndex) return;
		LastCheckedIndex = Settings->PrimitiveDataIndex;

		const UMaterialFunction* Fn = Cast<UMaterialFunction>(Settings->FadeFunction.LoadSynchronous());
		if (!Fn) return;

		for (const TObjectPtr<UMaterialExpression>& Expr : Fn->GetExpressions())
		{
			const UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(Expr);
			if (!Param || !Param->bUseCustomPrimitiveData) continue;

			if (Param->PrimitiveDataIndex != Settings->PrimitiveDataIndex)
			{
				UE_LOG(LogCS, Error, TEXT("CameraOcclusionFade: 설정의 PrimitiveDataIndex(%d) 와 %s 의 파라미터 '%s' 가 읽는 인덱스(%d) 가 다르다. 값을 넣어도 머티리얼이 읽지 못한다."),
					Settings->PrimitiveDataIndex, *Fn->GetName(), *Param->ParameterName.ToString(), Param->PrimitiveDataIndex);
			}
			return;
		}
		UE_LOG(LogCS, Warning, TEXT("CameraOcclusionFade: %s 안에 Custom Primitive Data 를 읽는 스칼라 파라미터가 없다."), *Fn->GetName());
	}
}
#endif

void UCSCameraOcclusionFadeSubsystem::ValidateActorMaterials(AActor* Actor)
{
#if WITH_EDITOR
	ValidateFadeFunctionIndex();
#endif

	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh)) continue;
		const int32 NumSlots = Mesh->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			if (!Material || IsMaterialFadeReady(Material) || WarnedMaterials.Contains(Material)) continue;

			WarnedMaterials.Add(Material);

			FString Functions;
			for (const FMaterialFunctionInfo& Info : Material->GetCachedExpressionData().FunctionInfos)
			{
				Functions += (Info.Function ? Info.Function->GetName() : TEXT("null")) + TEXT(",");
			}
			UE_LOG(LogCS, Warning,
				TEXT("CameraOcclusionFade: %s (액터 %s) 는 Masked 가 아니거나 OpacityMask 에 MF_CameraFade 가 없다. 이 머티리얼은 가려져도 투명해지지 않는다. (BlendMode=%d, 함수=[%s])"),
				*Material->GetPathName(), *Actor->GetName(), static_cast<int32>(Material->GetBlendMode()), *Functions);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 틱
// ─────────────────────────────────────────────────────────────────────────────

void UCSCameraOcclusionFadeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;

	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	if (!Settings || !Settings->bEnabled)
	{
		RestoreAll();
		return;
	}

	// 1) 목표값 초기화. 이번 틱에 안 걸리면 0 으로 남아 페이드아웃된다.
	for (auto& Pair : Faded)
	{
		Pair.Value.TargetFade = 0.f;
	}

	// 2) 모든 뷰에 대해 막힌 비율 판정
	TArray<FViewPair> Views;
	CollectViews(Views);

	if (Views.Num() > 0)
	{
		// 파괴된 액터는 GC 가 null 로 만든다. 여기서 걷어내 UpdateTargets 가 null 을 보지 않게 한다.
		Targets.RemoveAll([](const TObjectPtr<AActor>& A) { return !IsValid(A); });

		for (const FViewPair& View : Views)
		{
			UpdateTargets(View, Targets);
		}
	}

	// 3) 시간 보간 + 적용 + 정리
	for (auto It = Faded.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key();
		FCSCameraOcclusionFadeEntry& Entry = It.Value();

		if (!IsValid(Actor))
		{
			It.RemoveCurrent();
			continue;
		}

		const float Speed = (Entry.TargetFade > Entry.CurrentFade) ? Settings->FadeInSpeed : Settings->FadeOutSpeed;
		Entry.CurrentFade = FMath::FInterpConstantTo(Entry.CurrentFade, Entry.TargetFade, DeltaTime, Speed);

		if (Entry.CurrentFade <= KINDA_SMALL_NUMBER && Entry.TargetFade <= KINDA_SMALL_NUMBER)
		{
			RestoreEntry(Entry);
			It.RemoveCurrent();
			continue;
		}

		ApplyFadeAmount(Entry, Entry.CurrentFade);
	}

#if !UE_BUILD_SHIPPING
	if (CSCameraOcclusionFade::CVarDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		FString Msg = FString::Printf(TEXT("CameraFade: 뷰 %d, 후보 %d, 페이드 중 %d"), Views.Num(), Targets.Num(), Faded.Num());
		for (const auto& Pair : Faded)
		{
			Msg += FString::Printf(TEXT("\n  %s  목표 %.2f  현재 %.2f  메시 %d"),
				*GetNameSafe(Pair.Key), Pair.Value.TargetFade, Pair.Value.CurrentFade, Pair.Value.Meshes.Num());
		}
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.f, FColor::Cyan, Msg);
	}
#endif
}

void UCSCameraOcclusionFadeSubsystem::CollectViews(TArray<FViewPair>& OutViews) const
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 메인 뷰: 이 머신의 로컬 플레이어 카메라 → 조작 중인 폰
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) continue;

		APawn* Pawn = PC->GetPawn();
		if (!IsValid(Pawn)) continue;

		OutViews.Add({ PC->PlayerCameraManager->GetCameraLocation(), Pawn });
	}

	// 스플릿 보조 뷰: 원격 플레이어의 카메라 → 원격 캐릭터. 화면에 그려질 때만.
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UCSSplitScreenSubsystem* Split = GI->GetSubsystem<UCSSplitScreenSubsystem>())
		{
			FVector SecondaryCam;
			ACSCharacterPlayer* SecondaryTarget = nullptr;
			if (Split->TryGetVisibleSecondaryView(SecondaryCam, SecondaryTarget))
			{
				OutViews.Add({ SecondaryCam, SecondaryTarget });
			}
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::BuildSamplePoints(const FViewPair& View, TArray<FVector>& OutPoints)
{
	const ACharacter* Character = Cast<ACharacter>(View.Target);
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;

	FVector Center;
	float HalfHeight, Radius;
	if (Capsule)
	{
		Center = Capsule->GetComponentLocation();
		HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Radius = Capsule->GetScaledCapsuleRadius();
	}
	else if (View.Target)
	{
		FVector Extent;
		View.Target->GetActorBounds(true, Center, Extent);
		HalfHeight = Extent.Z;
		Radius = FMath::Max(Extent.X, Extent.Y);
	}
	else
	{
		return;
	}

	// 카메라에서 본 좌우 방향으로 열을 펼친다
	const FVector ToTarget = (Center - View.CameraLocation).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();

	OutPoints.Reset();
	for (float Row : CSCameraOcclusionFade::RowFractions)
	{
		for (float Col : CSCameraOcclusionFade::ColFractions)
		{
			OutPoints.Add(Center + FVector(0.f, 0.f, HalfHeight * Row) + Right * (Radius * Col));
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::UpdateTargets(const FViewPair& View, const TArray<TObjectPtr<AActor>>& Candidates)
{
	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const float ProbeRadius = Settings ? Settings->ProbeRadius : 12.f;
	const FCollisionShape Probe = FCollisionShape::MakeSphere(ProbeRadius);

	TArray<FVector> Points;
	BuildSamplePoints(View, Points);
	if (Points.Num() == 0) return;

	// 전체 샘플을 감싸는 상자. 액터 바운드와 겹치지 않으면 스윕조차 하지 않는다.
	FBox RayBounds(ForceInit);
	RayBounds += View.CameraLocation;
	for (const FVector& P : Points) RayBounds += P;

	TInlineComponentArray<UStaticMeshComponent*> Meshes;

	for (const TObjectPtr<AActor>& Actor : Candidates)
	{
		if (!IsValid(Actor) || Actor == View.Target) continue;

		Meshes.Reset();
		Actor->GetComponents<UStaticMeshComponent>(Meshes);

		TArray<UStaticMeshComponent*, TInlineAllocator<8>> Testable;
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->bHiddenInGame) continue;
			if (Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
			if (!Mesh->Bounds.GetBox().ExpandBy(ProbeRadius).Intersect(RayBounds)) continue;
			Testable.Add(Mesh);
		}
		if (Testable.Num() == 0) continue;

		// 광선마다 "이 액터의 메시 중 하나라도 막는가" 를 센다. 액터 단위로 페이드해야
		// 기어 원통만 반투명해지고 위에 붙은 버튼은 불투명하게 남는 반쪽 현상이 없다.
		int32 Blocked = 0;
		for (const FVector& End : Points)
		{
			for (UStaticMeshComponent* Mesh : Testable)
			{
				FHitResult Hit;
				if (Mesh->SweepComponent(Hit, View.CameraLocation, End, FQuat::Identity, Probe))
				{
					++Blocked;
					break;
				}
			}
		}
		if (Blocked == 0) continue;

		const float Coverage = static_cast<float>(Blocked) / static_cast<float>(Points.Num());

		FCSCameraOcclusionFadeEntry* Entry = Faded.Find(Actor);
		if (!Entry)
		{
			BeginFade(Actor);
			Entry = Faded.Find(Actor);
			if (!Entry) continue;
		}
		// 뷰가 여럿이면 가장 많이 가리는 뷰 기준
		Entry->TargetFade = FMath::Max(Entry->TargetFade, Coverage);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 적용 / 복원
// ─────────────────────────────────────────────────────────────────────────────

void UCSCameraOcclusionFadeSubsystem::BeginFade(AActor* Actor)
{
	// 등록 뒤 게임플레이가 머티리얼을 바꿨을 수 있다 (버튼 점등 등). 페이드가 시작될 때 한 번 더 본다.
	// 머티리얼당 한 번만 경고하므로 비용은 조회 몇 번이다.
	ValidateActorMaterials(Actor);

	FCSCameraOcclusionFadeEntry& Entry = Faded.Add(Actor);

	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->bHiddenInGame) continue;
		Entry.Meshes.Add(Mesh);
	}
}

void UCSCameraOcclusionFadeSubsystem::ApplyFadeAmount(FCSCameraOcclusionFadeEntry& Entry, float Amount)
{
	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const float MinOpacity = Settings ? Settings->MinOpacity : 0.15f;
	const int32 Index = Settings ? Settings->PrimitiveDataIndex : 0;

	// MF_CameraFade: 0 = 불투명, 1 = 완전 투명. 최소 불투명도만큼은 남긴다.
	const float Value = FMath::Clamp(Amount, 0.f, 1.f) * (1.f - MinOpacity);

	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : Entry.Meshes)
	{
		if (UStaticMeshComponent* Mesh = Weak.Get())
		{
			Mesh->SetCustomPrimitiveDataFloat(Index, Value);
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::RestoreEntry(const FCSCameraOcclusionFadeEntry& Entry)
{
	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	const int32 Index = Settings ? Settings->PrimitiveDataIndex : 0;

	for (const TWeakObjectPtr<UStaticMeshComponent>& Weak : Entry.Meshes)
	{
		if (UStaticMeshComponent* Mesh = Weak.Get())
		{
			Mesh->SetCustomPrimitiveDataFloat(Index, 0.f);
		}
	}
}

void UCSCameraOcclusionFadeSubsystem::RestoreAll()
{
	for (auto& Pair : Faded)
	{
		RestoreEntry(Pair.Value);
	}
	Faded.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// 에디터 콘솔 명령
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_EDITOR
namespace
{
	/**
	 * CS.CameraFade.FixMaterials
	 *
	 * UMaterial 에는 "OpacityMask 가 사실상 항상 1이면 Masked 여도 Opaque 로 취급" 하는 캐시 플래그
	 * bCanMaskedBeAssumedOpaque 가 있다. Opaque 로 만들어진 머티리얼은 이 값이 true 로 저장돼 있고,
	 * 나중에 Masked 로 바꾸고 MF_CameraFade 를 꽂아도 이 플래그는 다시 계산되지 않는다 (5.8 기준 엔진에 재계산 코드가 없다).
	 * 켜진 채로 두면 GetBlendMode() 가 Opaque 를 돌려줘 셰이더가 마스크를 무시하고 페이드가 되지 않는다.
	 *
	 * 프로젝트의 UMaterial 전부를 읽어, Masked 이고 MF_CameraFade 를 참조하는데 이 플래그가 켜진 것을 내리고
	 * 재컴파일한 뒤 더티로 표시한다. 저장은 하지 않는다 (Ctrl+S 또는 MCP save_assets).
	 */
	bool FixCameraFadeMaterials()
	{
		const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
		const UMaterialFunctionInterface* Fn = Settings ? Settings->FadeFunction.LoadSynchronous() : nullptr;
		if (!Fn)
		{
			UE_LOG(LogCS, Error, TEXT("CameraFade: 설정의 FadeFunction 을 로드하지 못했다."));
			return false;
		}

		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);

		int32 Scanned = 0, Fixed = 0;
		TArray<FString> FixedNames;
		for (const FAssetData& Data : Assets)
		{
			UMaterial* Material = Cast<UMaterial>(Data.GetAsset());
			if (!Material) continue;
			++Scanned;

			if (Material->BlendMode != BLEND_Masked) continue;

			bool bUsesFade = false;
			for (const FMaterialFunctionInfo& Info : Material->GetCachedExpressionData().FunctionInfos)
			{
				if (Info.Function == Fn) { bUsesFade = true; break; }
			}
			if (!bUsesFade) continue;

			// 임포트된 머티리얼은 OpacityMask 입력에 UseConstant 가 켜진 채 들어오기도 한다. 그러면 노드를 꽂아도
			// 컴파일러는 연결 대신 상수(1)를 써서 마스크가 죽는다. 머티리얼 에디터에서 와이어를 연결하면 UI 가 꺼 주지만
			// 코드로 연결한 경우는 남는다.
			UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
			const bool bConstOverride = EditorOnly && EditorOnly->OpacityMask.Expression && EditorOnly->OpacityMask.UseConstant;
			const bool bAssumedOpaque = Material->bCanMaskedBeAssumedOpaque;
			if (!bConstOverride && !bAssumedOpaque) continue;

			Material->PreEditChange(nullptr);
			Material->bCanMaskedBeAssumedOpaque = false;
			if (bConstOverride) EditorOnly->OpacityMask.UseConstant = false;
			Material->PostEditChange();	// 셰이더 재컴파일
			Material->MarkPackageDirty();
			++Fixed;
			FixedNames.Add(FString::Printf(TEXT("%s(%s%s)"), *Material->GetName(),
				bAssumedOpaque ? TEXT("AssumedOpaque") : TEXT(""), bConstOverride ? TEXT(" UseConstant") : TEXT("")));
		}

		UE_LOG(LogCS, Display, TEXT("CameraFade: 머티리얼 %d 개 검사, %d 개 수정 (더티 상태, 저장 필요): %s"),
			Scanned, Fixed, *FString::Join(FixedNames, TEXT(", ")));
		return true;
	}

	FAutoConsoleCommand GFixCameraFadeMaterialsCmd(
		TEXT("CS.CameraFade.FixMaterials"),
		TEXT("MF_CameraFade 를 쓰는 Masked 머티리얼의 bCanMaskedBeAssumedOpaque 와 OpacityMask.UseConstant 를 내리고 재컴파일한다 (저장은 별도)."),
		FConsoleCommandDelegate::CreateLambda([]() { FixCameraFadeMaterials(); }));
}
#endif // WITH_EDITOR
