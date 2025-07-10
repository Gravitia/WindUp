// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset/CSCharacterScaleData.h"

UCSCharacterScaleData::UCSCharacterScaleData()
{
	ScaleSmall = 0.5f;
	RadiusSmall = 16.0f;
	HalfHeightSmall = 32.0f;

	ScaleNormal = 1.0f;
	RadiusNormal = 32.0f;
	HalfHeightNormal = 88.0f;

	ScaleLarge = 1.5f;
	RadiusLarge = 64.0f;
	HalfHeightLarge = 196.0f;
}
