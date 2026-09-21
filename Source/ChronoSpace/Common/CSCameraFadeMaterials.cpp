// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/CSCameraFadeMaterials.h"
#include "DataAsset/CSCameraFadeMaterialTable.h"
#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "ChronoSpace.h"

#if WITH_EDITOR
#include "Editor/CSCameraFadeMaterialExtractor.h"
#include "Materials/MaterialInstance.h"

namespace
{
	/**
	 * 원본이 편집됐는지 판정하는 키. 부모 UMaterial 의 StateId 에 인스턴스 체인의 파라미터 오버라이드 해시를 섞는다.
	 * 인스턴스의 오버라이드(색·스칼라·텍스처)만 바꾸면 부모 StateId 는 그대로라서, 부모만 보면 PIE 내내 낡은 추출을 쓴다.
	 * 추출기가 읽는 것이 바로 이 오버라이드 값들이므로 값을 직접 해시하는 게 정확하다.
	 */
	FGuid ComputeStateKey(const UMaterialInterface* Original)
	{
		const UMaterial* Base = Original->GetMaterial();
		FGuid Key = Base ? Base->StateId : FGuid();

		uint32 Hash = 0;
		for (const UMaterialInstance* MI = Cast<UMaterialInstance>(Original); MI; MI = Cast<UMaterialInstance>(MI->Parent))
		{
			for (const FScalarParameterValue& P : MI->ScalarParameterValues)
			{
				Hash = HashCombine(Hash, HashCombine(GetTypeHash(P.ParameterInfo.Name), GetTypeHash(P.ParameterValue)));
			}
			for (const FVectorParameterValue& P : MI->VectorParameterValues)
			{
				Hash = HashCombine(Hash, HashCombine(GetTypeHash(P.ParameterInfo.Name), GetTypeHash(P.ParameterValue)));
			}
			for (const FTextureParameterValue& P : MI->TextureParameterValues)
			{
				Hash = HashCombine(Hash, HashCombine(GetTypeHash(P.ParameterInfo.Name), PointerHash(P.ParameterValue.Get())));
			}
			Hash = HashCombine(Hash, 0x9E3779B9u);	// 체인 단계 경계
		}
		Key.D ^= Hash;
		return Key;
	}
}
#endif

const TCHAR* FCSCameraFadeMaterials::TablePackageName = TEXT("/Game/_Generated/CameraFade/DA_CameraFadeMaterials");

TStrongObjectPtr<UCSCameraFadeMaterialTable> FCSCameraFadeMaterials::LoadedTable;
bool FCSCameraFadeMaterials::bTableLoadTried = false;

#if WITH_EDITOR
TMap<TWeakObjectPtr<UMaterialInterface>, FCSCameraFadeMaterialData> FCSCameraFadeMaterials::Cache;
TMap<TWeakObjectPtr<UMaterialInterface>, FCSCameraFadeMaterials::FFailed> FCSCameraFadeMaterials::Failed;
#endif

FString FCSCameraFadeMaterials::GetTableObjectPath()
{
	const FString Package(TablePackageName);
	return Package + TEXT(".") + FPackageName::GetShortName(Package);
}

UCSCameraFadeMaterialTable* FCSCameraFadeMaterials::LoadTable()
{
	if (LoadedTable.IsValid()) return LoadedTable.Get();
	if (bTableLoadTried) return nullptr;
	bTableLoadTried = true;

	UCSCameraFadeMaterialTable* Table = LoadObject<UCSCameraFadeMaterialTable>(nullptr, *GetTableObjectPath(), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Table)
	{
		LoadedTable.Reset(Table);
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("CameraFade: 추출 표 %s 가 없다. 쿠킹 때 생성되지 않았다면 어떤 오브젝트도 페이드하지 않는다."), *GetTableObjectPath());
	}
	return Table;
}

UMaterialInterface* FCSCameraFadeMaterials::GetFadeMaterial()
{
	if (const UCSCameraOcclusionFadeSettings* Settings = UCSCameraOcclusionFadeSettings::Get())
	{
		if (UMaterialInterface* FromSettings = Settings->FadeMaterial.LoadSynchronous()) return FromSettings;
	}
#if !WITH_EDITOR
	if (UCSCameraFadeMaterialTable* Table = LoadTable()) return Table->FadeMaterial;
#endif
	return nullptr;
}

const FCSCameraFadeMaterialData* FCSCameraFadeMaterials::Resolve(UMaterialInterface* Original, FString* OutWhy)
{
	if (!Original)
	{
		if (OutWhy) *OutWhy = TEXT("머티리얼이 없다");
		return nullptr;
	}

#if WITH_EDITOR
	const FGuid StateId = ComputeStateKey(Original);

	if (FCSCameraFadeMaterialData* Cached = Cache.Find(Original))
	{
		if (Cached->SourceStateId == StateId) return Cached;
		Cache.Remove(Original);
	}
	if (const FFailed* Prev = Failed.Find(Original))
	{
		if (Prev->StateId == StateId)
		{
			if (OutWhy) *OutWhy = Prev->Why;
			return nullptr;
		}
		Failed.Remove(Original);
	}

	FCSCameraFadeMaterialData Data;
	FString Why;
	if (FCSCameraFadeMaterialExtractor::Extract(Original, Data, Why))
	{
		Data.SourceStateId = StateId;
		return &Cache.Add(Original, Data);
	}
	Failed.Add(Original, FFailed{ StateId, Why });
	if (OutWhy) *OutWhy = Why;
	return nullptr;
#else
	UCSCameraFadeMaterialTable* Table = LoadTable();
	if (!Table)
	{
		if (OutWhy) *OutWhy = TEXT("추출 표가 없다");
		return nullptr;
	}
	const FCSCameraFadeMaterialData* Found = Table->Entries.Find(FSoftObjectPath(Original));
	if (!Found && OutWhy) *OutWhy = TEXT("추출 표에 없다 (쿠킹 때 수집되지 않았거나 지원 범위 밖)");
	return Found;
#endif
}

void FCSCameraFadeMaterials::ClearCache()
{
#if WITH_EDITOR
	Cache.Empty();
	Failed.Empty();
#endif
}
