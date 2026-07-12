// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CSAudioRoutingSettings.generated.h"

class USoundClass;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ChronoSpace Audio Routing"))
class CHRONOSPACE_API UCSAudioRoutingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category = "Sound Classes")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditAnywhere, Config, Category = "Sound Classes")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditAnywhere, Config, Category = "Sound Classes")
	TSoftObjectPtr<USoundClass> AmbientSoundClass;

	UPROPERTY(EditAnywhere, Config, Category = "Sound Classes")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(EditAnywhere, Config, Category = "Sound Classes")
	TSoftObjectPtr<USoundClass> UISoundClass;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UCSAudioRoutingSettings* Get();
};
