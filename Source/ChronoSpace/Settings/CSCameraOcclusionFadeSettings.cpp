// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "Materials/MaterialInterface.h"

UCSCameraOcclusionFadeSettings::UCSCameraOcclusionFadeSettings()
{
	FadeMaterial = FSoftObjectPath(TEXT("/Game/31_Material/CameraFade/M_CameraOcclusionFade.M_CameraOcclusionFade"));
}

const UCSCameraOcclusionFadeSettings* UCSCameraOcclusionFadeSettings::Get()
{
	return GetDefault<UCSCameraOcclusionFadeSettings>();
}
