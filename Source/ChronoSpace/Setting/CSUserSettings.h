// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "CSUserSettings.generated.h"

class USoundMix;

/**
 * 
 */
UCLASS(BlueprintType)
class CHRONOSPACE_API UCSUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
	UCSUserSettings();

	UFUNCTION(BlueprintPure, Category = "Settings")
	static UCSUserSettings* GetCSUserSettings();

	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void ApplyNonResolutionSettings() override;

// Sound
public:
	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetMasterVolume() { return MasterVolume; }

	UFUNCTION(BlueprintCallable)
	void SetMasterVolume(float InMasterVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetMusicVolume() { return MusicVolume; }

	UFUNCTION(BlueprintCallable)
	void SetMusicVolume(float InMusicVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetEffectsVolume() { return EffectsVolume; }

	UFUNCTION(BlueprintCallable)
	void SetEffectsVolume(float InEffectsVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetUIVolume() { return UIVolume; }

	UFUNCTION(BlueprintCallable)
	void SetUIVolume(float InUIVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetAmbientVolume() { return AmbientVolume; }

	UFUNCTION(BlueprintCallable)
	void SetAmbientVolume(float InAmbientVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetBGMVolume() { return GetMusicVolume(); }

	UFUNCTION(BlueprintCallable)
	void SetBGMVolume(float InBGMVolume);

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetSFXVolume() { return GetEffectsVolume(); }

	UFUNCTION(BlueprintCallable)
	void SetSFXVolume(float InSFXVolume);

protected:
	UPROPERTY(Config)
	float MasterVolume;

	UPROPERTY(Config)
	float MusicVolume;

	UPROPERTY(Config)
	float EffectsVolume;

	UPROPERTY(Config)
	float UIVolume;

	UPROPERTY(Config)
	float AmbientVolume;

	UPROPERTY(Config)
	float BGMVolume;

	UPROPERTY(Config)
	float SFXVolume;

	UPROPERTY(Config)
	int32 AudioSettingsVersion;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> RuntimeSoundMix;

private:
	bool MigrateLegacyAudioVolumeSettings();
	void ApplyAudioVolumeSettings();

	TSet<uint32> RuntimeSoundMixDeviceIds;
};
