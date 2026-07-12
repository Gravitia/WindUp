// Fill out your copyright notice in the Description page of Project Settings.


#include "Setting/CSUserSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Settings/CSAudioRoutingSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
	constexpr int32 CurrentAudioSettingsVersion = 1;

	void SetSoundClassOverride(FAudioDevice& AudioDevice, USoundMix* SoundMix, const TSoftObjectPtr<USoundClass>& SoundClassReference, float Volume)
	{
		if (USoundClass* SoundClass = SoundClassReference.LoadSynchronous())
		{
			AudioDevice.SetSoundMixClassOverride(SoundMix, SoundClass, Volume, 1.0f, 0.0f, true);
		}
	}
}

UCSUserSettings::UCSUserSettings()
{
	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	EffectsVolume = 1.0f;
	UIVolume = 1.0f;
	AmbientVolume = 1.0f;
	BGMVolume = 1.0f;
	SFXVolume = 1.0f;
	AudioSettingsVersion = 0;
}

UCSUserSettings* UCSUserSettings::GetCSUserSettings()
{
	return GEngine ? Cast<UCSUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UCSUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	if (MigrateLegacyAudioVolumeSettings())
	{
		SaveSettings();
	}
}

void UCSUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.0f, 1.0f);
	UIVolume = FMath::Clamp(UIVolume, 0.0f, 1.0f);
	AmbientVolume = FMath::Clamp(AmbientVolume, 0.0f, 1.0f);
	BGMVolume = MusicVolume;
	SFXVolume = EffectsVolume;
	ApplyAudioVolumeSettings();
}


void UCSUserSettings::SetMasterVolume(float InMasterVolume)
{
	MasterVolume = FMath::Clamp(InMasterVolume, 0.0f, 1.0f);
	ApplyAudioVolumeSettings();
	SaveConfig();
}

void UCSUserSettings::SetMusicVolume(float InMusicVolume)
{
	MusicVolume = FMath::Clamp(InMusicVolume, 0.0f, 1.0f);
	BGMVolume = MusicVolume;
	ApplyAudioVolumeSettings();
	SaveConfig();
}

void UCSUserSettings::SetEffectsVolume(float InEffectsVolume)
{
	EffectsVolume = FMath::Clamp(InEffectsVolume, 0.0f, 1.0f);
	SFXVolume = EffectsVolume;
	ApplyAudioVolumeSettings();
	SaveConfig();
}

void UCSUserSettings::SetUIVolume(float InUIVolume)
{
	UIVolume = FMath::Clamp(InUIVolume, 0.0f, 1.0f);
	ApplyAudioVolumeSettings();
	SaveConfig();
}

void UCSUserSettings::SetAmbientVolume(float InAmbientVolume)
{
	AmbientVolume = FMath::Clamp(InAmbientVolume, 0.0f, 1.0f);
	ApplyAudioVolumeSettings();
	SaveConfig();
}

void UCSUserSettings::SetBGMVolume(float InBGMVolume)
{
	SetMusicVolume(InBGMVolume);
}

void UCSUserSettings::SetSFXVolume(float InSFXVolume)
{
	SetEffectsVolume(InSFXVolume);
}

bool UCSUserSettings::MigrateLegacyAudioVolumeSettings()
{
	if (AudioSettingsVersion >= CurrentAudioSettingsVersion)
	{
		return false;
	}

	MusicVolume = FMath::Clamp(BGMVolume, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
	BGMVolume = MusicVolume;
	SFXVolume = EffectsVolume;
	AudioSettingsVersion = CurrentAudioSettingsVersion;
	return true;
}

void UCSUserSettings::ApplyAudioVolumeSettings()
{
	FAudioDeviceManager* AudioDeviceManager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;
	if (!AudioDeviceManager)
	{
		return;
	}

	if (!RuntimeSoundMix)
	{
		RuntimeSoundMix = NewObject<USoundMix>(this, TEXT("CSRuntimeAudioOptionsMix"), RF_Transient);
	}

	const UCSAudioRoutingSettings* RoutingSettings = UCSAudioRoutingSettings::Get();
	AudioDeviceManager->IterateOverAllDevices([this, RoutingSettings](Audio::FDeviceId AudioDeviceId, FAudioDevice* AudioDevice)
	{
		if (!AudioDevice)
		{
			return;
		}

		SetSoundClassOverride(*AudioDevice, RuntimeSoundMix, RoutingSettings->MasterSoundClass, MasterVolume);
		SetSoundClassOverride(*AudioDevice, RuntimeSoundMix, RoutingSettings->MusicSoundClass, MusicVolume);
		SetSoundClassOverride(*AudioDevice, RuntimeSoundMix, RoutingSettings->AmbientSoundClass, AmbientVolume);
		SetSoundClassOverride(*AudioDevice, RuntimeSoundMix, RoutingSettings->EffectsSoundClass, EffectsVolume);
		SetSoundClassOverride(*AudioDevice, RuntimeSoundMix, RoutingSettings->UISoundClass, UIVolume);

		if (!RuntimeSoundMixDeviceIds.Contains(AudioDeviceId))
		{
			AudioDevice->PushSoundMixModifier(RuntimeSoundMix);
			RuntimeSoundMixDeviceIds.Add(AudioDeviceId);
		}
	});
}
