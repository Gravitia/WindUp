// Fill out your copyright notice in the Description page of Project Settings.

#include "Editor/CSCameraFadeTableGenerator.h"

#if WITH_EDITOR

#include "Editor/CSCameraFadeMaterialExtractor.h"
#include "Common/CSCameraFadeMaterials.h"
#include "DataAsset/CSCameraFadeMaterialTable.h"
#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "ChronoSpace.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/ARFilter.h"
#include "UObject/ICookInfo.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

TArray<FName> FCSCameraFadeTableGenerator::CookPackageNames;
bool FCSCameraFadeTableGenerator::bCookGenerated = false;

namespace
{
	// 외부에서 가져온 팩. 이 프로젝트의 대상 액터가 쓰지 않고, 로드 비용만 든다.
	const TCHAR* ExcludedFolders[] = {
		TEXT("/Game/05_ThirdPerson/"),
		TEXT("/Game/07_LyraCharacter/"),
		TEXT("/Game/EasyGameUI/"),
		TEXT("/Game/DemonicUI/"),
		TEXT("/Game/AdvancedMenu/"),
		TEXT("/Game/USCS/"),
		TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/"),
		TEXT("/Game/Jet_engine_effects/"),
		TEXT("/Game/_Generated/"),
	};

	bool IsExcludedPackage(const FString& PackageName)
	{
		if (!PackageName.StartsWith(TEXT("/Game/"))) return true;
		for (const TCHAR* Folder : ExcludedFolders)
		{
			if (PackageName.StartsWith(Folder)) return true;
		}
		return false;
	}
}

void FCSCameraFadeTableGenerator::RegisterHooks()
{
	FEditorDelegates::PreBeginPIE.AddStatic(&FCSCameraFadeTableGenerator::OnPreBeginPIE);
	FEditorDelegates::EndPIE.AddStatic(&FCSCameraFadeTableGenerator::OnEndPIE);
	UE::Cook::FDelegates::CookStarted.AddStatic(&FCSCameraFadeTableGenerator::OnCookStarted);
	UE::Cook::FDelegates::ModifyCook.AddStatic(&FCSCameraFadeTableGenerator::OnModifyCook);
}

// ─────────────────────────────────────────────────────────────────────────────
// 훅
// ─────────────────────────────────────────────────────────────────────────────

void FCSCameraFadeTableGenerator::OnPreBeginPIE(bool /*bIsSimulating*/)
{
	if (!GEditor) return;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	TSet<UMaterialInterface*> Originals;
	CollectFromWorld(World, Originals);

	int32 Ok = 0;
	for (UMaterialInterface* Original : Originals)
	{
		FString Why;
		if (FCSCameraFadeMaterials::Resolve(Original, &Why))
		{
			++Ok;
		}
		else
		{
			UE_LOG(LogCS, Warning, TEXT("CameraFade: %s 는 페이드하지 않는다 - %s"), *Original->GetPathName(), *Why);
		}
	}
	UE_LOG(LogCS, Log, TEXT("CameraFade: PIE 준비 - 머티리얼 %d/%d 추출됨"), Ok, Originals.Num());
}

void FCSCameraFadeTableGenerator::OnEndPIE(bool /*bIsSimulating*/)
{
	FCSCameraFadeMaterials::ClearCache();
}

void FCSCameraFadeTableGenerator::OnCookStarted(UE::Cook::ICookInfo& /*CookInfo*/)
{
	EnsureGeneratedForCook();
}

void FCSCameraFadeTableGenerator::OnModifyCook(UE::Cook::ICookInfo& /*CookInfo*/, TArray<UE::Cook::FPackageCookRule>& InOutRules)
{
	// CookStarted 와 ModifyCook 의 순서에 기대지 않는다. 먼저 불린 쪽이 생성한다.
	EnsureGeneratedForCook();

	for (const FName& PackageName : CookPackageNames)
	{
		UE::Cook::FPackageCookRule& Rule = InOutRules.AddDefaulted_GetRef();
		Rule.PackageName = PackageName;
		Rule.InstigatorName = TEXT("CSCameraFadeTableGenerator");
		Rule.CookRule = UE::Cook::EPackageCookRule::AddToCook;
	}
}

void FCSCameraFadeTableGenerator::EnsureGeneratedForCook()
{
	// 성공했을 때만 잠근다. 실패하면 다음 훅(CookStarted ↔ ModifyCook)이 한 번 더 시도하고, 그래도 안 되면
	// Error 로그가 쿠킹을 실패로 끝낸다. 실패 상태로 잠그면 그 뒤 쿠킹이 규칙 없이 지나간다.
	if (bCookGenerated) return;
	CookPackageNames.Reset();

	// 여기서 로드하는 것은 전부 "표를 만들기 위한" 로드다. 쿠커가 이 로드를 보고 그 패키지를 쿠킹에 넣지 않게 한다.
	// 표가 쿠킹되면 표의 하드 참조(페이드 머티리얼)는 자연히 함께 쿠킹된다.
	FCookLoadScope LoadScope(ECookLoadType::EditorOnly);

	const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get();
	UMaterialInterface* FadeMaterial = Settings ? Settings->FadeMaterial.LoadSynchronous() : nullptr;
	if (!FadeMaterial)
	{
		UE_LOG(LogCS, Error, TEXT("CameraFade: 설정의 FadeMaterial 을 로드하지 못했다. 추출 표를 만들지 않는다."));
		return;
	}

	TArray<UMaterialInterface*> Originals;
	CollectProjectMaterials(Originals);

	const FString PackageName(FCSCameraFadeMaterials::TablePackageName);
	const FString ObjectName = FPackageName::GetShortName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	bool bCreated = false;
	UCSCameraFadeMaterialTable* Table = FindObject<UCSCameraFadeMaterialTable>(Package, *ObjectName);
	if (!Table)
	{
		Table = NewObject<UCSCameraFadeMaterialTable>(Package, *ObjectName, RF_Public | RF_Standalone);
		bCreated = true;
	}

	Table->Modify();
	Table->FadeMaterial = FadeMaterial;
	Table->Entries.Reset();

	int32 Failed = 0;
	for (UMaterialInterface* Original : Originals)
	{
		FCSCameraFadeMaterialData Data;
		FString Why;
		if (FCSCameraFadeMaterialExtractor::Extract(Original, Data, Why))
		{
			Table->Entries.Add(FSoftObjectPath(Original), Data);
		}
		else
		{
			++Failed;
			// 프로젝트 머티리얼 전부를 훑으므로 대상이 아닌 것(반투명, 포스트프로세스 등)도 많이 걸린다. Log 로 남긴다.
			UE_LOG(LogCS, Log, TEXT("CameraFade: %s 제외 - %s"), *Original->GetPathName(), *Why);
		}
	}

	Package->MarkPackageDirty();
	if (bCreated) FAssetRegistryModule::AssetCreated(Table);

	const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FileName), true);

	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Table, *FileName, Args))
	{
		UE_LOG(LogCS, Error, TEXT("CameraFade: 추출 표 저장 실패 %s"), *FileName);
		return;
	}

	// 쿠커는 "디스크에 있는 패키지" 를 에셋 레지스트리로 판단한다. 방금 저장한 파일을 레지스트리에 알려야
	// AddToCook 이 "Unable to find package" 로 거부되지 않는다.
	IAssetRegistry::Get()->ScanModifiedAssetFiles({ FPaths::ConvertRelativePathToFull(FileName) });

	CookPackageNames.Add(Package->GetFName());
	bCookGenerated = true;
	UE_LOG(LogCS, Display, TEXT("CameraFade: 쿠킹 준비 - 머티리얼 %d 개 중 %d 개 추출, %d 개 제외 → %s"),
		Originals.Num(), Table->Entries.Num(), Failed, *PackageName);
}

// ─────────────────────────────────────────────────────────────────────────────
// 수집
// ─────────────────────────────────────────────────────────────────────────────

void FCSCameraFadeTableGenerator::CollectFromActor(AActor* Actor, TSet<UMaterialInterface*>& Out)
{
	// 페이드 대상으로 등록하는 컴포넌트들 (UCSCameraOcclusionFadeSubsystem::RegisterTarget 호출자와 맞춘다)
	if (!IsValid(Actor)) return;
	if (!Actor->FindComponentByClass<UCSMeshPulledByBlackhole>() &&
		!Actor->FindComponentByClass<UCSMeshAffectedByGravityCore>()) return;

	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const int32 Num = Mesh->GetNumMaterials();
		for (int32 i = 0; i < Num; ++i)
		{
			if (UMaterialInterface* M = Mesh->GetMaterial(i)) Out.Add(M);
		}
	}
}

void FCSCameraFadeTableGenerator::CollectFromWorld(UWorld* World, TSet<UMaterialInterface*>& Out)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		CollectFromActor(*It, Out);
	}
}

void FCSCameraFadeTableGenerator::CollectProjectMaterials(TArray<UMaterialInterface*>& Out)
{
	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	Registry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	for (const FAssetData& Data : Assets)
	{
		if (IsExcludedPackage(Data.PackageName.ToString())) continue;
		if (UMaterialInterface* M = Cast<UMaterialInterface>(Data.GetAsset())) Out.Add(M);
	}
}

#endif // WITH_EDITOR
