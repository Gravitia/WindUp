// Copyright Epic Games, Inc. All Rights Reserved.


#include "EIK_Login_AsyncFunction.h"

#include "EIKSettings.h"
#include "Online.h"
#include "Misc/CommandLine.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemEIK/SdkFunctions/ConnectInterface/EIK_ConnectSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"

namespace
{
	FString MakeValidEosDisplayName(const FString& DisplayName)
	{
		return DisplayName.TrimStartAndEnd().Left(32);
	}

	FString GetFallbackDeviceLoginId()
	{
		FString CommandLineLoginId;
		if (FParse::Value(FCommandLine::Get(), TEXT("LoginId="), CommandLineLoginId) && !CommandLineLoginId.TrimStartAndEnd().IsEmpty())
		{
			return MakeValidEosDisplayName(CommandLineLoginId);
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

		return MakeValidEosDisplayName(StoredGuid);
	}
}

UEIK_Login_AsyncFunction* UEIK_Login_AsyncFunction::LoginUsingConnectInterface(TEnumAsByte<EEIK_EExternalCredentialType> LoginMethod, FString DisplayName, FString Token)
{
	UEIK_Login_AsyncFunction* UEIK_LoginObject= NewObject<UEIK_Login_AsyncFunction>();
	const FString NormalizedDisplayName = DisplayName.TrimStartAndEnd();
	UEIK_LoginObject->DisplayName = NormalizedDisplayName.IsEmpty() || NormalizedDisplayName.Equals(TEXT("AUTO_"), ESearchCase::IgnoreCase)
		? GetFallbackDeviceLoginId()
		: MakeValidEosDisplayName(NormalizedDisplayName);
	UEIK_LoginObject->Token = Token;
	UEIK_LoginObject->LoginMethod = LoginMethod;
	return UEIK_LoginObject;
}

void UEIK_Login_AsyncFunction::Activate()
{
	Super::Activate();
	if(const IOnlineSubsystem *SubsystemRef = Online::GetSubsystem(GetWorld(), "EIK"))
	{
		if(const IOnlineIdentityPtr IdentityPointerRef = SubsystemRef->GetIdentityInterface())
		{
			UE_LOG(LogEIK, Verbose, TEXT("LoginUsingConnectInterface: Subsystem and Identity Interface is valid and proceeding with login."));
			FString EGS_Token;
			FOnlineAccountCredentials AccountDetails;
			AccountDetails.Id = DisplayName;
			AccountDetails.Token = Token;
			AccountDetails.Type = "noeas_+_" + UEnum::GetValueAsString(LoginMethod);
			UE_LOG(LogEIK, Log, TEXT("LoginUsingConnectInterface: Login Method: %s"), *AccountDetails.Type);
			IdentityPointerRef->OnLoginCompleteDelegates->AddUObject(this,&UEIK_Login_AsyncFunction::LoginCallback);
			IdentityPointerRef->Login(0,AccountDetails);
			return;
		}
	}
	UE_LOG(LogEIK, Error, TEXT("LoginUsingConnectInterface: Subsystem or Identity Interface is not valid"));
	OnFail.Broadcast(FEIK_ProductUserId(), "Subsystem or Identity Interface is not valid");
	SetReadyToDestroy();
#if ENGINE_MAJOR_VERSION == 5
	MarkAsGarbage();
#else
	MarkPendingKill();
#endif
}

void UEIK_Login_AsyncFunction::LoginCallback(int32 LocalUserNum, bool bWasSuccess, const FUniqueNetId& UserId,
	const FString& Error)
{
	if(bWasSuccess)
	{
		if(UserId.IsValid())
		{
			UE_LOG(LogEIK, Verbose, TEXT("LoginUsingConnectInterface: Login was successful. UserID: %s"), *UserId.ToString());
			OnSuccess.Broadcast(EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserId.ToString())), "");
		}
		else
		{
			UE_LOG(LogEIK, Error, TEXT("LoginUsingConnectInterface: Login was successful but UserID is not valid"));
			OnFail.Broadcast(FEIK_ProductUserId(), "UserID is not valid");
		}
	}
	else
	{
		UE_LOG(LogEIK, Error, TEXT("LoginUsingConnectInterface: Login failed. Error: %s"), *Error);
		OnFail.Broadcast(FEIK_ProductUserId(), Error);
	}
	SetReadyToDestroy();
#if ENGINE_MAJOR_VERSION == 5
	MarkAsGarbage();
#else
	MarkPendingKill();
#endif
}
