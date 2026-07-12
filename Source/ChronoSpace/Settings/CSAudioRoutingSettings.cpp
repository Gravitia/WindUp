// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/CSAudioRoutingSettings.h"

const UCSAudioRoutingSettings* UCSAudioRoutingSettings::Get()
{
	return GetDefault<UCSAudioRoutingSettings>();
}
