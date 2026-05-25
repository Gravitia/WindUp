// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "CSUserSettings.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class CHRONOSPACE_API UCSUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
	UCSUserSettings();

	virtual void ApplySettings(bool bForce = false) override;

// Sound
public:
	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetMasterVolume() { return MasterVolume; }

	UFUNCTION(BlueprintCallable)
	void SetMasterVolume(float InMasterVolume);

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetBGMVolume() { return BGMVolume; }

	UFUNCTION(BlueprintCallable)
	void SetBGMVolume(float InBGMVolume);

	UFUNCTION(BlueprintCallable)
	FORCEINLINE float GetSFXVolume() { return SFXVolume; }

	UFUNCTION(BlueprintCallable)
	void SetSFXVolume(float InSFXVolume);

protected:
	UPROPERTY(Config)
	float MasterVolume;

	UPROPERTY(Config)
	float BGMVolume;

	UPROPERTY(Config)
	float SFXVolume;
};
