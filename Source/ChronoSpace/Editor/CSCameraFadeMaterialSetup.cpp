// Fill out your copyright notice in the Description page of Project Settings.

// 카메라 가림 페이드용 머티리얼 일괄 설정 (에디터 전용 콘솔 명령).
//
//   CS.CameraFade.SetupMaterials [save] [current]
//     페이드 대상 액터(UCSMeshPulledByBlackhole / UCSMeshAffectedByGravityCore 가 붙은 것)가 쓰는
//     부모 Material 을 전부 찾아 MF_CameraFade 를 꽂고 Masked 로 바꾼 뒤 숨은 플래그를 정리한다.
//       - 기본: 프로젝트의 블루프린트 전부 + 맵 전부(서브레벨, 월드 파티션 외부 액터 포함)를 훑는다.
//       - current: 지금 에디터에 열린 월드의 액터만 훑는다. 빠르다.
//       - save:    바꾼 머티리얼 패키지를 바로 저장한다. 없으면 더티만 남긴다.
//
//   CS.CameraFade.FixMaterials
//     이미 MF_CameraFade 를 쓰는 머티리얼의 숨은 플래그를 정리하고 (아래 "왜 플래그 정리가 필요한가"),
//     연결·Masked 전환이 빠진 것도 마저 처리한다.
//
// 머티리얼별 처리 규칙:
//   Opaque                         → 함수 호출 노드 추가, OpacityMask 에 연결, Masked 로 전환
//   Masked + 기존 OpacityMask 있음  → Multiply(기존 마스크, 함수) 를 OpacityMask 에 연결. 원래 구멍은 유지된다.
//   이미 함수 있음 + Masked + OpacityMask 연결됨 → 건너뜀 (플래그만 검사)
//   이미 함수 있음, 그러나 연결이나 Masked 전환이 빠짐 → 남은 단계만 채운다 (노드는 다시 안 꽂는다)
//   Translucent 등 그 외 블렌드     → 건너뛰고 보고. 유리처럼 이미 비치는 재질이거나 Opacity 를 따로 다뤄야 한다.
//   Surface 도메인이 아니거나 MaterialAttributes 사용 → 건너뛰고 보고
//   /Game 밖(엔진·플러그인) 또는 서드파티 팩 머티리얼 → 건너뛰고 보고. 프로젝트 폴더로 복제해서 쓴다.
//
// 왜 플래그 정리가 필요한가:
//   1) UMaterial::bCanMaskedBeAssumedOpaque — Opaque 시절 캐시. 켜져 있으면 Masked 여도 Opaque 로 컴파일된다.
//      5.8 엔진에는 이 값을 다시 계산하는 코드가 없다.
//   2) OpacityMask 입력의 UseConstant — FBX 임포터가 켜 둔다. 켜져 있으면 꽂은 노드 대신 상수 1을 쓴다.
//      머티리얼 에디터에서 손으로 와이어를 꽂으면 UI 가 꺼 주지만 코드로 꽂으면 남는다.
//   둘 다 리플렉션(MCP ObjectTools)으로는 읽거나 쓸 수 없어서 C++ 이 필요하다.

#if WITH_EDITOR

#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "ChronoSpace.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialCachedData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"

namespace CSCameraFadeSetup
{
	// 외부에서 가져온 팩. 대상 액터가 쓰지 않고 로드 비용만 든다.
	const TCHAR* ExcludedFolders[] = {
		TEXT("/Game/05_ThirdPerson/"),
		TEXT("/Game/07_LyraCharacter/"),
		TEXT("/Game/EasyGameUI/"),
		TEXT("/Game/DemonicUI/"),
		TEXT("/Game/AdvancedMenu/"),
		TEXT("/Game/USCS/"),
		TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/"),
		TEXT("/Game/Jet_engine_effects/"),
	};

	bool IsExcluded(const FString& PackageName)
	{
		if (!PackageName.StartsWith(TEXT("/Game/"))) return true;
		for (const TCHAR* Folder : ExcludedFolders)
		{
			if (PackageName.StartsWith(Folder)) return true;
		}
		return false;
	}

	bool IsTargetComponentClass(const UClass* Class)
	{
		return Class && (Class->IsChildOf<UCSMeshPulledByBlackhole>() || Class->IsChildOf<UCSMeshAffectedByGravityCore>());
	}

	// ── 수집 ───────────────────────────────────────────────────────────────────

	struct FCollected
	{
		/** 부모 Material → 그것을 쓰는 곳 설명 (보고용, 최대 몇 개만) */
		TMap<UMaterial*, TArray<FString>> Materials;
		int32 Actors = 0;
		int32 Blueprints = 0;
		int32 Maps = 0;

		void Add(UMaterialInterface* Interface, const FString& Where)
		{
			UMaterial* Base = Interface ? Interface->GetMaterial() : nullptr;
			if (!Base) return;
			TArray<FString>& Users = Materials.FindOrAdd(Base);
			if (Users.Num() < 3) Users.AddUnique(Where);
		}
	};

	void CollectFromMesh(UStaticMeshComponent* Mesh, const FString& Where, FCollected& Out)
	{
		if (!Mesh) return;
		const int32 Num = Mesh->GetNumMaterials();
		for (int32 i = 0; i < Num; ++i)
		{
			Out.Add(Mesh->GetMaterial(i), Where);
		}
	}

	/** 레벨에 놓인 액터. 인스턴스로 추가한 컴포넌트까지 GetComponents 가 본다. */
	void CollectFromActor(AActor* Actor, FCollected& Out)
	{
		if (!IsValid(Actor)) return;

		bool bTarget = false;
		for (UActorComponent* C : Actor->GetComponents())
		{
			if (C && IsTargetComponentClass(C->GetClass())) { bTarget = true; break; }
		}
		if (!bTarget) return;

		++Out.Actors;
		TInlineComponentArray<UStaticMeshComponent*> Meshes;
		Actor->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			CollectFromMesh(Mesh, Actor->GetName(), Out);
		}
	}

	/** 블루프린트 클래스. SCS 노드(부모 체인 포함)와 CDO 의 네이티브 컴포넌트를 본다. */
	void CollectFromBlueprintClass(UBlueprintGeneratedClass* Class, FCollected& Out)
	{
		if (!Class) return;

		bool bTarget = false;
		TArray<UStaticMeshComponent*> MeshTemplates;

		for (UClass* C = Class; C; C = C->GetSuperClass())
		{
			UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(C);
			if (!BPGC) break;
			if (BPGC->SimpleConstructionScript)
			{
				for (USCS_Node* Node : BPGC->SimpleConstructionScript->GetAllNodes())
				{
					UActorComponent* Template = Node ? Node->ComponentTemplate : nullptr;
					if (!Template) continue;
					if (IsTargetComponentClass(Template->GetClass())) bTarget = true;
					if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Template)) MeshTemplates.Add(Mesh);
				}
			}
		}

		if (AActor* CDO = Cast<AActor>(Class->GetDefaultObject()))
		{
			for (UActorComponent* C : CDO->GetComponents())
			{
				if (!C) continue;
				if (IsTargetComponentClass(C->GetClass())) bTarget = true;
				if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(C)) MeshTemplates.Add(Mesh);
			}
		}

		if (!bTarget) return;

		++Out.Blueprints;
		for (UStaticMeshComponent* Mesh : MeshTemplates)
		{
			CollectFromMesh(Mesh, Class->GetName(), Out);
		}
	}

	void CollectFromAllBlueprints(FCollected& Out)
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		for (const FAssetData& Data : Assets)
		{
			if (IsExcluded(Data.PackageName.ToString())) continue;
			UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset());
			if (!BP) continue;
			CollectFromBlueprintClass(Cast<UBlueprintGeneratedClass>(BP->GeneratedClass), Out);
		}
	}

	void CollectFromWorld(UWorld* World, FCollected& Out)
	{
		if (!World) return;
		++Out.Maps;
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level) continue;
			for (AActor* Actor : Level->Actors)
			{
				CollectFromActor(Actor, Out);
			}
		}
	}

	/** 월드 파티션 맵의 액터는 맵 패키지가 아니라 외부 패키지에 있다. 레지스트리에서 그 폴더의 액터 에셋을 로드한다. */
	void CollectFromExternalActors(const FString& MapPackageName, FCollected& Out)
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		for (const FString& Path : ULevel::GetExternalActorsPaths(MapPackageName))
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPath(FName(*Path), Assets, true);
			for (const FAssetData& Data : Assets)
			{
				CollectFromActor(Cast<AActor>(Data.GetAsset()), Out);
			}
		}
	}

	void CollectFromAllMaps(FCollected& Out)
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		FARFilter Filter;
		Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		for (const FAssetData& Data : Assets)
		{
			const FString PackageName = Data.PackageName.ToString();
			if (IsExcluded(PackageName)) continue;

			UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
			UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
			if (!World)
			{
				UE_LOG(LogCS, Warning, TEXT("CameraFade: 맵을 열지 못했다 %s"), *PackageName);
				continue;
			}
			CollectFromWorld(World, Out);
			CollectFromExternalActors(PackageName, Out);
		}
	}

	// ── 머티리얼 처리 ───────────────────────────────────────────────────────────

	enum class EResult : uint8 { Inserted, Combined, Completed, AlreadyDone, FlagsFixed, Skipped };

	bool UsesFunction(const UMaterial* Material, const UMaterialFunctionInterface* Fn)
	{
		for (const FMaterialFunctionInfo& Info : Material->GetCachedExpressionData().FunctionInfos)
		{
			if (Info.Function == Fn) return true;
		}
		return false;
	}

	/** 그래프에 이미 들어 있는 페이드 함수 호출 노드. 없으면 null. */
	UMaterialExpressionMaterialFunctionCall* FindFunctionCall(const UMaterialEditorOnlyData* EditorOnly, const UMaterialFunctionInterface* Fn)
	{
		for (UMaterialExpression* Expr : EditorOnly->ExpressionCollection.Expressions)
		{
			UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expr);
			if (Call && Call->MaterialFunction == Fn) return Call;
		}
		return nullptr;
	}

	UMaterialExpressionMaterialFunctionCall* AddFunctionCall(UMaterial* Material, UMaterialEditorOnlyData* EditorOnly, UMaterialFunctionInterface* Fn)
	{
		UMaterialExpressionMaterialFunctionCall* Call = NewObject<UMaterialExpressionMaterialFunctionCall>(Material, NAME_None, RF_Transactional);
		Call->Material = Material;
		Call->MaterialExpressionEditorX = -400;
		Call->MaterialExpressionEditorY = 900;
		EditorOnly->ExpressionCollection.AddExpression(Call);
		Call->SetMaterialFunction(Fn);
		return Call;
	}

	/** 한 머티리얼을 규칙대로 처리한다. 바꾼 게 없으면 AlreadyDone 을 돌려주고 머티리얼을 건드리지 않는다. */
	EResult SetupMaterial(UMaterial* Material, UMaterialFunctionInterface* Fn, FString& OutWhy)
	{
		// 엔진(/Engine), 플러그인, 서드파티 팩 머티리얼은 건드리지 않는다. 대상 액터가 그걸 쓰고 있으면 보고만 한다.
		if (IsExcluded(Material->GetOutermost()->GetName()))
		{
			OutWhy = TEXT("프로젝트 밖(엔진/플러그인/서드파티 팩) 머티리얼. 프로젝트 폴더로 복제해서 쓴다");
			return EResult::Skipped;
		}
		if (Material->MaterialDomain != MD_Surface) { OutWhy = TEXT("Surface 도메인이 아니다"); return EResult::Skipped; }
		if (Material->bUseMaterialAttributes) { OutWhy = TEXT("MaterialAttributes 를 쓴다"); return EResult::Skipped; }

		UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
		if (!EditorOnly) { OutWhy = TEXT("에디터 데이터가 없다"); return EResult::Skipped; }

		const bool bHasFunction = UsesFunction(Material, Fn);
		const EBlendMode Blend = EBlendMode(Material->BlendMode);

		if (Blend != BLEND_Opaque && Blend != BLEND_Masked)
		{
			OutWhy = FString::Printf(TEXT("블렌드 모드 %d (Opaque/Masked 만 자동 처리)"), static_cast<int32>(Blend));
			return EResult::Skipped;
		}

		// 바꿀 게 있는지 먼저 판단한다. 없으면 PreEditChange/PostEditChange 를 부르지 않는다.
		// UMaterial::PostEditChange() 는 프로퍼티 없이 불러도 셰이더 ID 를 새로 만들고(DDC 무효화)
		// 씬 전체 컴포넌트의 렌더 스테이트를 다시 만든다. 이미 된 머티리얼 38개에 그걸 반복하면 안 된다.
		const bool bNeedFlagFix =
			Material->bCanMaskedBeAssumedOpaque ||
			(EditorOnly->OpacityMask.Expression && EditorOnly->OpacityMask.UseConstant);

		// "함수가 있다" 만으로는 부족하다. 노드만 놓고 OpacityMask 연결이나 Masked 전환을 빠뜨린 상태(수동 작업 중단)를
		// 여기서 걸러야 두 명령 다 "이미 됨" 으로 지나치지 않는다.
		const bool bWired = bHasFunction && Blend == BLEND_Masked && EditorOnly->OpacityMask.Expression != nullptr;

		if (bWired && !bNeedFlagFix)
		{
			return EResult::AlreadyDone;
		}

		EResult Result = EResult::FlagsFixed;
		Material->PreEditChange(nullptr);

		if (!bHasFunction)
		{
			UMaterialExpressionMaterialFunctionCall* Call = AddFunctionCall(Material, EditorOnly, Fn);

			if (Blend == BLEND_Masked && EditorOnly->OpacityMask.Expression)
			{
				// 기존 마스크 × 페이드. 둘 다 0/1 이라 원래 구멍은 남고 페이드 중엔 디더가 더해진다.
				UMaterialExpressionMultiply* Mul = NewObject<UMaterialExpressionMultiply>(Material, NAME_None, RF_Transactional);
				Mul->Material = Material;
				Mul->MaterialExpressionEditorX = -200;
				Mul->MaterialExpressionEditorY = 900;
				EditorOnly->ExpressionCollection.AddExpression(Mul);
				Mul->A.Connect(EditorOnly->OpacityMask.OutputIndex, EditorOnly->OpacityMask.Expression);
				Mul->B.Connect(0, Call);
				EditorOnly->OpacityMask.Connect(0, Mul);
				Result = EResult::Combined;
			}
			else
			{
				EditorOnly->OpacityMask.Connect(0, Call);
				Material->BlendMode = BLEND_Masked;
				Result = EResult::Inserted;
			}
		}
		else if (!bWired)
		{
			// 함수 노드는 있는데 마무리가 안 된 상태. 노드를 다시 꽂지 않고 남은 단계만 채운다.
			if (!EditorOnly->OpacityMask.Expression)
			{
				UMaterialExpressionMaterialFunctionCall* Call = FindFunctionCall(EditorOnly, Fn);
				if (!Call) Call = AddFunctionCall(Material, EditorOnly, Fn);	// 캐시에는 있는데 그래프에 없는 경우(있을 수 없지만 방어)
				EditorOnly->OpacityMask.Connect(0, Call);
			}
			Material->BlendMode = BLEND_Masked;
			Result = EResult::Completed;
		}

		// 숨은 플래그 정리 (파일 상단 설명 참고). 방금 함수를 꽂은 경우도 여기서 같이 내린다.
		Material->bCanMaskedBeAssumedOpaque = false;
		if (EditorOnly->OpacityMask.Expression) { EditorOnly->OpacityMask.UseConstant = false; }

		Material->PostEditChange();	// 셰이더 재컴파일
		Material->MarkPackageDirty();
		return Result;
	}

	void SavePackages(const TArray<UPackage*>& Packages)
	{
		for (UPackage* Package : Packages)
		{
			const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, nullptr, *FileName, Args))
			{
				UE_LOG(LogCS, Error, TEXT("CameraFade: 저장 실패 %s"), *FileName);
			}
		}
	}

	// ── 명령 ───────────────────────────────────────────────────────────────────

	void SetupMaterials(const TArray<FString>& Args)
	{
		const bool bSave = Args.Contains(TEXT("save"));
		const bool bCurrentOnly = Args.Contains(TEXT("current"));

		const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
		UMaterialFunctionInterface* Fn = Settings ? Settings->FadeFunction.LoadSynchronous() : nullptr;
		if (!Fn)
		{
			UE_LOG(LogCS, Error, TEXT("CameraFade: 설정의 FadeFunction 을 로드하지 못했다. 프로젝트 설정 > ChronoSpace Camera Occlusion Fade 를 확인한다."));
			return;
		}

		FCollected Collected;
		if (bCurrentOnly)
		{
			// UnrealEd 의존 없이 에디터 월드를 찾는다
			UWorld* EditorWorld = nullptr;
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::Editor) { EditorWorld = Context.World(); break; }
				}
			}
			CollectFromWorld(EditorWorld, Collected);
		}
		else
		{
			IAssetRegistry::GetChecked().SearchAllAssets(true);
			CollectFromAllBlueprints(Collected);
			CollectFromAllMaps(Collected);
		}

		int32 Inserted = 0, Combined = 0, Completed = 0, Done = 0, Flags = 0, Skipped = 0;
		TArray<UPackage*> Dirty;
		for (auto& Pair : Collected.Materials)
		{
			UMaterial* Material = Pair.Key;
			FString Why;
			switch (SetupMaterial(Material, Fn, Why))
			{
			case EResult::Inserted:   ++Inserted; Dirty.AddUnique(Material->GetOutermost()); UE_LOG(LogCS, Display, TEXT("CameraFade: 삽입 %s"), *Material->GetPathName()); break;
			case EResult::Combined:   ++Combined; Dirty.AddUnique(Material->GetOutermost()); UE_LOG(LogCS, Display, TEXT("CameraFade: 기존 마스크와 합성 %s"), *Material->GetPathName()); break;
			case EResult::Completed:  ++Completed; Dirty.AddUnique(Material->GetOutermost()); UE_LOG(LogCS, Display, TEXT("CameraFade: 함수는 있었고 연결/Masked 마저 처리 %s"), *Material->GetPathName()); break;
			case EResult::FlagsFixed: ++Flags;    Dirty.AddUnique(Material->GetOutermost()); UE_LOG(LogCS, Display, TEXT("CameraFade: 플래그 정리 %s"), *Material->GetPathName()); break;
			case EResult::AlreadyDone: ++Done; break;
			case EResult::Skipped:
				++Skipped;
				UE_LOG(LogCS, Warning, TEXT("CameraFade: 건너뜀 %s - %s (쓰는 곳: %s)"), *Material->GetPathName(), *Why, *FString::Join(Pair.Value, TEXT(", ")));
				break;
			}
		}

		if (bSave) SavePackages(Dirty);

		UE_LOG(LogCS, Display,
			TEXT("CameraFade: 설정 완료 - 블루프린트 %d, 맵 %d, 대상 액터 %d, 부모 머티리얼 %d | 삽입 %d, 합성 %d, 마저 처리 %d, 플래그 정리 %d, 이미 됨 %d, 건너뜀 %d%s"),
			Collected.Blueprints, Collected.Maps, Collected.Actors, Collected.Materials.Num(),
			Inserted, Combined, Completed, Flags, Done, Skipped,
			bSave ? TEXT(" (저장함)") : (Dirty.Num() > 0 ? TEXT(" (더티 상태, 저장 필요)") : TEXT("")));

		if (!bCurrentOnly)
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	void FixMaterials()
	{
		const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
		UMaterialFunctionInterface* Fn = Settings ? Settings->FadeFunction.LoadSynchronous() : nullptr;
		if (!Fn)
		{
			UE_LOG(LogCS, Error, TEXT("CameraFade: 설정의 FadeFunction 을 로드하지 못했다."));
			return;
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
			if (!UsesFunction(Material, Fn)) continue;	// 함수를 쓰는 머티리얼만. 블렌드 모드는 SetupMaterial 이 판단한다.

			FString Why;
			const EResult Result = SetupMaterial(Material, Fn, Why);
			if (Result == EResult::FlagsFixed || Result == EResult::Completed)
			{
				++Fixed;
				FixedNames.Add(Material->GetName() + (Result == EResult::Completed ? TEXT("(마저 처리)") : TEXT("")));
			}
		}

		UE_LOG(LogCS, Display, TEXT("CameraFade: 함수 쓰는 머티리얼 %d 개 검사, %d 개 정리 (더티 상태, 저장 필요): %s"),
			Scanned, Fixed, *FString::Join(FixedNames, TEXT(", ")));
	}

	FAutoConsoleCommand GSetupCmd(
		TEXT("CS.CameraFade.SetupMaterials"),
		TEXT("페이드 대상 액터의 부모 머티리얼에 MF_CameraFade 를 꽂고 Masked 로 바꾸고 플래그를 정리한다. 인자: save(저장), current(열린 월드만)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetupMaterials));

	FAutoConsoleCommand GFixCmd(
		TEXT("CS.CameraFade.FixMaterials"),
		TEXT("MF_CameraFade 를 쓰는 머티리얼의 숨은 플래그(bCanMaskedBeAssumedOpaque, OpacityMask.UseConstant)를 내리고, OpacityMask 연결이나 Masked 전환이 빠졌으면 마저 처리한다 (저장은 별도)."),
		FConsoleCommandDelegate::CreateStatic(&FixMaterials));
}

#endif // WITH_EDITOR
