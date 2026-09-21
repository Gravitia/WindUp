// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class UMaterialInterface;
class UWorld;
class AActor;
namespace UE::Cook { class ICookInfo; struct FPackageCookRule; }

/**
 * 카메라 페이드용 추출 표 생성기. 에디터 빌드에만 존재한다.
 *
 *  - FEditorDelegates::PreBeginPIE : 열린 레벨의 대상 액터 머티리얼을 미리 읽어 메모리 캐시를 덥힌다.
 *                                    빠진 것(언로드된 WP 셀, 런타임 스폰)은 서브시스템이 처음 필요할 때 읽는다.
 *  - FEditorDelegates::EndPIE      : 캐시를 비운다. 세션 중 원본이 편집됐을 수 있다.
 *  - UE::Cook::CookStarted/ModifyCook : 프로젝트의 머티리얼 에셋 전부를 읽어 UCSCameraFadeMaterialTable 을
 *                                    Content/_Generated/CameraFade/ 에 저장하고 쿠킹 목록에 넣는다.
 *
 * 쿠킹 때 블루프린트·맵·액터는 로드하지 않는다. 어떤 액터가 어떤 머티리얼을 쓰는지 알아내려고
 * 블루프린트를 로드하면 (1) 그 블루프린트가 쿠킹에 딸려 들어가고 (2) 깨진 블루프린트의 컴파일 에러가
 * 쿠킹 실패로 잡힌다. 머티리얼만 읽으면 둘 다 없고, 표가 프로젝트의 모든 머티리얼을 담으니 누락도 없다.
 * 로드는 FCookLoadScope(EditorOnly) 안에서 해서 쿠킹 대상에 영향을 주지 않는다.
 */
struct FCSCameraFadeTableGenerator
{
	static void RegisterHooks();

private:
	static void OnPreBeginPIE(bool bIsSimulating);
	static void OnEndPIE(bool bIsSimulating);
	static void OnCookStarted(UE::Cook::ICookInfo& CookInfo);
	static void OnModifyCook(UE::Cook::ICookInfo& CookInfo, TArray<UE::Cook::FPackageCookRule>& InOutRules);

	static void EnsureGeneratedForCook();

	/** 열린 에디터 월드의 대상 액터가 쓰는 머티리얼 (PIE 워밍업용) */
	static void CollectFromWorld(UWorld* World, TSet<UMaterialInterface*>& Out);
	static void CollectFromActor(AActor* Actor, TSet<UMaterialInterface*>& Out);

	/** 프로젝트 콘텐츠의 머티리얼 에셋 전부 (서드파티 팩 폴더 제외). 레지스트리로 고르고 로드한다. */
	static void CollectProjectMaterials(TArray<UMaterialInterface*>& Out);

	static TArray<FName> CookPackageNames;
	static bool bCookGenerated;
};

#endif // WITH_EDITOR
