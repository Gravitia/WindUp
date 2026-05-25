// Fill out your copyright notice in the Description page of Project Settings.


#include "Setting/CSUserSettings.h"

UCSUserSettings::UCSUserSettings()
{
	MasterVolume = 1.0f;
	BGMVolume = 1.0f;
	SFXVolume = 1.0f;
	LoadConfig();
}

void UCSUserSettings::ApplySettings(bool bForce)
{
	Super::ApplySettings(bForce);
	
	SetMasterVolume(MasterVolume);
	SetBGMVolume(BGMVolume);
	SetSFXVolume(SFXVolume);

	SaveSettings();
}


void UCSUserSettings::SetMasterVolume(float InMasterVolume)
{
	MasterVolume = FMath::Clamp(InMasterVolume, 0.0f, 1.0f);
	SaveConfig();
}

void UCSUserSettings::SetBGMVolume(float InBGMVolume)
{
	BGMVolume = FMath::Clamp(InBGMVolume, 0.0f, 1.0f);
	SaveConfig();
}

void UCSUserSettings::SetSFXVolume(float InSFXVolume)
{
	SFXVolume = FMath::Clamp(InSFXVolume, 0.0f, 1.0f);
	SaveConfig();
}
