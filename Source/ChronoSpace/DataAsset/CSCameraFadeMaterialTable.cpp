// Fill out your copyright notice in the Description page of Project Settings.

#include "DataAsset/CSCameraFadeMaterialTable.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"

const FName FCSCameraFadeParams::FadeAmount(TEXT("FadeAmount"));
const FName FCSCameraFadeParams::MinOpacity(TEXT("MinOpacity"));
const FName FCSCameraFadeParams::UVTiling(TEXT("UVTiling"));
const FName FCSCameraFadeParams::BaseColorTexture(TEXT("BaseColorTexture"));
const FName FCSCameraFadeParams::BaseColorTint(TEXT("BaseColorTint"));
const FName FCSCameraFadeParams::UseBaseColorTexture(TEXT("UseBaseColorTexture"));
const FName FCSCameraFadeParams::EmissiveTexture(TEXT("EmissiveTexture"));
const FName FCSCameraFadeParams::EmissiveColor(TEXT("EmissiveColor"));
const FName FCSCameraFadeParams::UseEmissiveTexture(TEXT("UseEmissiveTexture"));
const FName FCSCameraFadeParams::NormalTexture(TEXT("NormalTexture"));
const FName FCSCameraFadeParams::UseNormalTexture(TEXT("UseNormalTexture"));
const FName FCSCameraFadeParams::RoughnessTexture(TEXT("RoughnessTexture"));
const FName FCSCameraFadeParams::RoughnessChannel(TEXT("RoughnessChannel"));
const FName FCSCameraFadeParams::RoughnessConstant(TEXT("RoughnessConstant"));
const FName FCSCameraFadeParams::UseRoughnessTexture(TEXT("UseRoughnessTexture"));
const FName FCSCameraFadeParams::MetallicTexture(TEXT("MetallicTexture"));
const FName FCSCameraFadeParams::MetallicChannel(TEXT("MetallicChannel"));
const FName FCSCameraFadeParams::MetallicConstant(TEXT("MetallicConstant"));
const FName FCSCameraFadeParams::UseMetallicTexture(TEXT("UseMetallicTexture"));

namespace
{
	/** 소프트 참조를 푼다. 원본 머티리얼이 로드돼 있으면 메모리에서 바로 찾고, 아니면 로드를 시도한다. */
	bool ResolveTexture(const TSoftObjectPtr<UTexture>& Soft, UTexture*& Out)
	{
		if (Soft.IsNull())
		{
			Out = nullptr;
			return true;
		}
		Out = Soft.LoadSynchronous();
		return Out != nullptr;
	}
}

bool FCSCameraFadeMaterialData::ApplyTo(UMaterialInstanceDynamic* MID) const
{
	if (!MID) return false;

	UTexture* BaseTex = nullptr;
	UTexture* EmissiveTex = nullptr;
	UTexture* NormalTex = nullptr;
	UTexture* RoughTex = nullptr;
	UTexture* MetalTex = nullptr;
	if (!ResolveTexture(BaseColorTexture, BaseTex) || !ResolveTexture(EmissiveTexture, EmissiveTex)
		|| !ResolveTexture(NormalTexture, NormalTex)
		|| !ResolveTexture(RoughnessTexture, RoughTex) || !ResolveTexture(MetallicTexture, MetalTex))
	{
		return false;
	}

	MID->SetVectorParameterValue(FCSCameraFadeParams::UVTiling, FLinearColor(UVTiling.X, UVTiling.Y, 0.f, 0.f));

	MID->SetVectorParameterValue(FCSCameraFadeParams::BaseColorTint, BaseColorTint);
	if (BaseTex) MID->SetTextureParameterValue(FCSCameraFadeParams::BaseColorTexture, BaseTex);
	MID->SetScalarParameterValue(FCSCameraFadeParams::UseBaseColorTexture, BaseTex ? 1.f : 0.f);

	MID->SetVectorParameterValue(FCSCameraFadeParams::EmissiveColor, EmissiveColor);
	if (EmissiveTex) MID->SetTextureParameterValue(FCSCameraFadeParams::EmissiveTexture, EmissiveTex);
	MID->SetScalarParameterValue(FCSCameraFadeParams::UseEmissiveTexture, EmissiveTex ? 1.f : 0.f);

	if (NormalTex) MID->SetTextureParameterValue(FCSCameraFadeParams::NormalTexture, NormalTex);
	MID->SetScalarParameterValue(FCSCameraFadeParams::UseNormalTexture, NormalTex ? 1.f : 0.f);

	MID->SetScalarParameterValue(FCSCameraFadeParams::RoughnessConstant, RoughnessConstant);
	MID->SetVectorParameterValue(FCSCameraFadeParams::RoughnessChannel, RoughnessChannel);
	if (RoughTex) MID->SetTextureParameterValue(FCSCameraFadeParams::RoughnessTexture, RoughTex);
	MID->SetScalarParameterValue(FCSCameraFadeParams::UseRoughnessTexture, RoughTex ? 1.f : 0.f);

	MID->SetScalarParameterValue(FCSCameraFadeParams::MetallicConstant, MetallicConstant);
	MID->SetVectorParameterValue(FCSCameraFadeParams::MetallicChannel, MetallicChannel);
	if (MetalTex) MID->SetTextureParameterValue(FCSCameraFadeParams::MetallicTexture, MetalTex);
	MID->SetScalarParameterValue(FCSCameraFadeParams::UseMetallicTexture, MetalTex ? 1.f : 0.f);
	return true;
}
