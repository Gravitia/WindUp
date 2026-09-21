// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/CSCameraOcclusionFadeSettings.h"
#include "Materials/MaterialFunctionInterface.h"

UCSCameraOcclusionFadeSettings::UCSCameraOcclusionFadeSettings()
{
	FadeFunction = FSoftObjectPath(TEXT("/Game/31_Material/CameraFade/MF_CameraFade.MF_CameraFade"));
}

const UCSCameraOcclusionFadeSettings* UCSCameraOcclusionFadeSettings::Get()
{
	return GetDefault<UCSCameraOcclusionFadeSettings>();
}
