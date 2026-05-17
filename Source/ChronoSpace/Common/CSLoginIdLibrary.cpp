// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/CSLoginIdLibrary.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Guid.h"

FString UCSLoginIdLibrary::GetPersistentDeviceLoginId()
{
	FString CommandLineLoginId;
	if (FParse::Value(FCommandLine::Get(), TEXT("LoginId="), CommandLineLoginId) && !CommandLineLoginId.TrimStartAndEnd().IsEmpty())
	{
		return CommandLineLoginId.TrimStartAndEnd().Left(32);
	}

	static constexpr TCHAR SectionName[] = TEXT("/Script/ChronoSpace.Login");
	static constexpr TCHAR KeyName[] = TEXT("PersistentDeviceLoginGuid");

	FString StoredGuid;
	if (!GConfig->GetString(SectionName, KeyName, StoredGuid, GGameUserSettingsIni) || StoredGuid.IsEmpty())
	{
		StoredGuid = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		GConfig->SetString(SectionName, KeyName, *StoredGuid, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	return StoredGuid.Left(32);
}
