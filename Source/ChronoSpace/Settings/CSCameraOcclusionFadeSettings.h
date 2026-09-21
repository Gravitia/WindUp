// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CSCameraOcclusionFadeSettings.generated.h"

class UMaterialFunctionInterface;

/**
 * 카메라와 캐릭터 사이를 막는 오브젝트를 반투명으로 페이드하는 UCSCameraOcclusionFadeSubsystem 의 설정.
 * 프로젝트 설정 > Game > ChronoSpace Camera Occlusion Fade.
 *
 * 페이드는 머티리얼 쪽이 그린다. 대상 오브젝트의 Material 은 Masked 이고 OpacityMask 에 MF_CameraFade 를 꽂아야 한다.
 * 그 함수는 메시 컴포넌트의 Custom Primitive Data 한 칸(PrimitiveDataIndex)을 읽어 디더 마스크를 만든다.
 * 코드는 그 칸에 값만 넣는다. 머티리얼을 바꾸거나 복제하지 않는다.
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
	 * 페이드 값을 넣는 Custom Primitive Data 인덱스. MF_CameraFade 안의 파라미터가 읽는 인덱스와 같아야 한다.
	 * 바꾸려면 함수 쪽도 같이 바꾼다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0", ClampMax = "31"))
	int32 PrimitiveDataIndex = 0;

	/**
	 * 대상 머티리얼이 꽂고 있어야 하는 페이드 함수. 에디터에서 PIE 시작 때 대상 액터의 머티리얼을 검사해
	 * 이 함수가 없거나 Masked 가 아니면 경고한다. 런타임 동작에는 쓰지 않는다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade")
	TSoftObjectPtr<UMaterialFunctionInterface> FadeFunction;

	/** 완전히 가렸을 때의 불투명도. 0 이면 완전 투명. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinOpacity = 0.15f;

	/**
	 * 가리기 시작했을 때 투명해지는 속도. 페이드 값(0 = 불투명, 1 = 최대 페이드)이 1초에 얼마나 변하는가.
	 * 4 면 0 → 1 전 구간을 0.25초에 간다.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.1"))
	float FadeInSpeed = 4.f;

	/** 시선에서 벗어났을 때 원래대로 돌아오는 속도. 같은 단위. 2 면 1 → 0 을 0.5초에 돌아온다. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "0.1"))
	float FadeOutSpeed = 2.f;

	/** 캐릭터 캡슐에 쏘는 샘플 광선의 구 반지름. 클수록 살짝 스치는 오브젝트도 잡힌다. */
	UPROPERTY(EditAnywhere, Config, Category = "CSEditable|CameraOcclusionFade", meta = (ClampMin = "1.0"))
	float ProbeRadius = 12.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UCSCameraOcclusionFadeSettings* Get();
};
