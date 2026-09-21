// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CSCameraOcclusionFadeSettings.generated.h"

class UMaterialInterface;

/**
 * 카메라와 캐릭터 사이를 막는 오브젝트를 반투명으로 페이드하는 UCSCameraOcclusionFadeSubsystem 의 설정.
 * 프로젝트 설정 > Game > ChronoSpace Camera Occlusion Fade.
 *
 * 페이드 머티리얼은 하나다. 원본 머티리얼에서 뽑아낸 텍스처·색을 파라미터로 받아 원본처럼 보인다
 * (FCSCameraFadeMaterials, UCSCameraFadeMaterialTable 참고).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ChronoSpace Camera Occlusion Fade"))
class CHRONOSPACE_API UCSCameraOcclusionFadeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCSCameraOcclusionFadeSettings();

	/** 끄면 아무것도 바꾸지 않는다 */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade")
	bool bEnabled = true;

	/**
	 * 가려진 오브젝트에 씌우는 머티리얼. Masked + DitherTemporalAA 이고 FCSCameraFadeParams 의 파라미터를 노출해야 한다.
	 * 기본값 M_CameraOcclusionFade 를 쓴다. 바꿀 일은 거의 없다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade")
	TSoftObjectPtr<UMaterialInterface> FadeMaterial;

	/** 완전히 가렸을 때(FadeAmount=1)의 불투명도. 0 이면 완전 투명. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinOpacity = 0.15f;

	/** 가리기 시작했을 때 투명해지는 속도 (FadeAmount/초). 4 면 0.25초에 완전 페이드. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.1"))
	float FadeInSpeed = 4.f;

	/** 시선에서 벗어났을 때 원래대로 돌아오는 속도 (FadeAmount/초) */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.1"))
	float FadeOutSpeed = 2.f;

	/** 캐릭터 캡슐에 쏘는 샘플 광선의 구 반지름. 클수록 살짝 스치는 오브젝트도 잡힌다. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "1.0"))
	float ProbeRadius = 12.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UCSCameraOcclusionFadeSettings* Get();
};
