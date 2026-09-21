// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UMaterialInterface;
class UCSCameraFadeMaterialTable;
struct FCSCameraFadeMaterialData;

/**
 * 원본 머티리얼 → 페이드용 추출 데이터 조회.
 *
 *  - 에디터/PIE: 원본 그래프를 그 자리에서 읽는다 (FCSCameraFadeMaterialExtractor). 결과는 메모리에 캐시하고
 *                원본의 StateId 가 바뀌면 다시 읽는다. 디스크에 아무것도 남기지 않는다.
 *  - 패키지:     쿠킹 때 만들어진 UCSCameraFadeMaterialTable 을 한 번 로드해 찾는다. 그래프는 패키지에 없다.
 */
struct CHRONOSPACE_API FCSCameraFadeMaterials
{
	/** 쿠킹 때 생성되는 표의 롱 패키지 이름. Content/_Generated/ 는 .gitignore 대상이다. */
	static const TCHAR* TablePackageName;

	/** 표의 오브젝트 경로 (패키지.이름) */
	static FString GetTableObjectPath();

	/** 페이드 머티리얼. 설정(UCSCameraOcclusionFadeSettings)의 것을 로드하고, 없으면 표의 것. */
	static UMaterialInterface* GetFadeMaterial();

	/**
	 * 원본에 대한 추출 데이터. 없으면 nullptr 이고 OutWhy 에 이유를 쓴다.
	 * 돌려준 포인터는 다음 Resolve 호출 전까지만 유효하다. 바로 ApplyTo 하고 들고 있지 않는다.
	 */
	static const FCSCameraFadeMaterialData* Resolve(UMaterialInterface* Original, FString* OutWhy = nullptr);

	/** 에디터 메모리 캐시를 비운다 (PIE 종료 시) */
	static void ClearCache();

private:
	static UCSCameraFadeMaterialTable* LoadTable();

	static TStrongObjectPtr<UCSCameraFadeMaterialTable> LoadedTable;
	static bool bTableLoadTried;

#if WITH_EDITOR
	struct FFailed
	{
		FGuid StateId;
		FString Why;
	};
	static TMap<TWeakObjectPtr<UMaterialInterface>, FCSCameraFadeMaterialData> Cache;
	static TMap<TWeakObjectPtr<UMaterialInterface>, FFailed> Failed;
#endif
};
