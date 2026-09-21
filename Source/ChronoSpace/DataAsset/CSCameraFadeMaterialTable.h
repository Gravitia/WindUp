// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSCameraFadeMaterialTable.generated.h"

class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** 페이드 머티리얼(M_CameraOcclusionFade)이 노출하는 파라미터 이름. 머티리얼 그래프와 같이 바꿔야 한다. */
struct CHRONOSPACE_API FCSCameraFadeParams
{
	static const FName FadeAmount;
	static const FName MinOpacity;
	static const FName UVTiling;
	static const FName BaseColorTexture;
	static const FName BaseColorTint;
	static const FName UseBaseColorTexture;
	static const FName EmissiveTexture;
	static const FName EmissiveColor;
	static const FName UseEmissiveTexture;
	static const FName NormalTexture;
	static const FName UseNormalTexture;
	static const FName RoughnessTexture;
	static const FName RoughnessChannel;
	static const FName RoughnessConstant;
	static const FName UseRoughnessTexture;
	static const FName MetallicTexture;
	static const FName MetallicChannel;
	static const FName MetallicConstant;
	static const FName UseMetallicTexture;
};

/**
 * 원본 머티리얼 하나에서 뽑아낸 값. 페이드 머티리얼에 이 값을 넣으면 원본과 같은 색·질감으로 그려진다.
 *
 * 에디터에서 원본 그래프를 읽어 만든다 (FCSCameraFadeMaterialExtractor). 추측은 없다.
 * 그래프가 지원 범위를 벗어나면 엔트리 자체가 만들어지지 않고 그 머티리얼은 페이드하지 않는다.
 *
 * 텍스처는 소프트 참조다. 이 데이터(와 쿠킹 표)가 텍스처를 쿠킹에 끌어들이지 않게 하기 위해서다.
 * 런타임에 페이드할 때 원본 머티리얼은 이미 로드돼 있으므로 그 텍스처도 메모리에 있다.
 */
USTRUCT()
struct FCSCameraFadeMaterialData
{
	GENERATED_BODY()

	/** 없으면 BaseColorTint 단색 */
	UPROPERTY()
	TSoftObjectPtr<UTexture> BaseColorTexture;

	/** 텍스처가 있으면 곱해지는 색, 없으면 색 그 자체 */
	UPROPERTY()
	FLinearColor BaseColorTint = FLinearColor::White;

	/** 없으면 EmissiveColor 단색 (기본 검정 = 발광 없음) */
	UPROPERTY()
	TSoftObjectPtr<UTexture> EmissiveTexture;

	/** 텍스처가 있으면 곱해지는 색, 없으면 발광색 그 자체. HDR(1 초과) 허용. */
	UPROPERTY()
	FLinearColor EmissiveColor = FLinearColor::Black;

	/** 없으면 평평한 노멀 */
	UPROPERTY()
	TSoftObjectPtr<UTexture> NormalTexture;

	UPROPERTY()
	TSoftObjectPtr<UTexture> RoughnessTexture;

	/** RoughnessTexture 의 어느 채널을 쓰는지 (R,G,B,A 중 하나가 1) */
	UPROPERTY()
	FLinearColor RoughnessChannel = FLinearColor(1.f, 0.f, 0.f, 0.f);

	UPROPERTY()
	float RoughnessConstant = 0.5f;

	UPROPERTY()
	TSoftObjectPtr<UTexture> MetallicTexture;

	UPROPERTY()
	FLinearColor MetallicChannel = FLinearColor(1.f, 0.f, 0.f, 0.f);

	UPROPERTY()
	float MetallicConstant = 0.f;

	/** 모든 텍스처에 공통으로 적용되는 UV 타일링 */
	UPROPERTY()
	FVector2D UVTiling = FVector2D(1.f, 1.f);

	/**
	 * 뽑아낼 당시 원본의 상태 키. 에디터 캐시의 낡음 판정에 쓴다.
	 * 부모 UMaterial::StateId 에 인스턴스 체인의 ParameterStateId 를 섞은 값이다 (FCSCameraFadeMaterials 가 채운다).
	 * 인스턴스의 파라미터 오버라이드만 바꿔도 바뀌어야 하므로 부모 StateId 만으로는 부족하다.
	 */
	UPROPERTY()
	FGuid SourceStateId;

	/** 페이드 머티리얼 MID 에 값을 전부 넣는다. 필요한 텍스처를 찾지 못하면 false. */
	bool ApplyTo(UMaterialInstanceDynamic* MID) const;
};

/**
 * 원본 머티리얼 → 추출 데이터 표. 패키지 빌드에서만 쓴다.
 *
 * 쿠킹 시작 때 FCSCameraFadeTableGenerator 가 프로젝트의 머티리얼 에셋 전부를 읽어 Content/_Generated/CameraFade/ 에
 * 저장하고 쿠킹 목록에 넣는다. 이 폴더는 .gitignore 대상이다. 에디터/PIE 는 이 표를 읽지 않고 원본 그래프를 직접 읽는다.
 * 어떤 머티리얼이 쿠킹되는지는 표와 무관하다. 표에 있어도 원본이 쿠킹되지 않았으면 그냥 안 쓰인다.
 *
 * FadeMaterial 을 하드 참조로 들고 있어 표가 쿠킹되면 페이드 머티리얼도 함께 쿠킹된다.
 */
UCLASS()
class CHRONOSPACE_API UCSCameraFadeMaterialTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FadeMaterial;

	UPROPERTY()
	TMap<FSoftObjectPath, FCSCameraFadeMaterialData> Entries;
};
