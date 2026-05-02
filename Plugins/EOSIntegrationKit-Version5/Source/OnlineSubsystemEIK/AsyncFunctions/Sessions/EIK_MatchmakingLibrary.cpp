// Copyright Epic Games, Inc. All Rights Reserved.

#include "EIK_MatchmakingLibrary.h"

namespace
{
	const FString PartyMatchKeySettingName = TEXT("PartyMatchKey");

	FString NormalizeMatchKey(const FString& Value)
	{
		return Value.TrimStartAndEnd().ToLower();
	}

	bool DoesAttributeMatch(const FEIKAttribute& Attribute, const FString& ExpectedKey)
	{
		if (Attribute.AttributeType != EEIKAttributeType::String)
		{
			return false;
		}

		return NormalizeMatchKey(Attribute.StringValue) == ExpectedKey;
	}

	bool DoesSettingMatch(
		const TMap<FString, FEIKAttribute>& SessionSettings,
		const FString& SettingName,
		const FString& ExpectedKey)
	{
		if (const FEIKAttribute* Attribute = SessionSettings.Find(SettingName))
		{
			return DoesAttributeMatch(*Attribute, ExpectedKey);
		}

		return false;
	}

	FString AttributeToString(const FEIKAttribute& Attribute)
	{
		switch (Attribute.AttributeType)
		{
		case EEIKAttributeType::String:
			return Attribute.StringValue;
		case EEIKAttributeType::Bool:
			return Attribute.BoolValue ? TEXT("true") : TEXT("false");
		case EEIKAttributeType::Integer:
			return FString::FromInt(Attribute.IntValue);
		default:
			return TEXT("<unknown>");
		}
	}

	FString FindStringSettingOrEmpty(const FSessionFindStruct& SessionResult, const FString& SettingName)
	{
		if (const FEIKAttribute* Attribute = SessionResult.SessionSettings.Find(SettingName))
		{
			return AttributeToString(*Attribute);
		}

		return FString();
	}
}

FEIKAttribute UEIK_MatchmakingLibrary::MakeEIKStringAttribute(const FString& Value)
{
	FEIKAttribute Attribute;
	Attribute.AttributeType = EEIKAttributeType::String;
	Attribute.StringValue = Value.TrimStartAndEnd();
	return Attribute;
}

void UEIK_MatchmakingLibrary::AddPartyMatchKeySetting(
	TMap<FString, FEIKAttribute>& SessionSettings,
	const FString& PartyMatchKey)
{
	const FString NormalizedPartyMatchKey = PartyMatchKey.TrimStartAndEnd();
	if (NormalizedPartyMatchKey.IsEmpty())
	{
		return;
	}

	SessionSettings.Add(PartyMatchKeySettingName, MakeEIKStringAttribute(NormalizedPartyMatchKey));
}

TMap<FString, FEIKAttribute> UEIK_MatchmakingLibrary::WithPartyMatchKeySetting(
	const TMap<FString, FEIKAttribute>& SessionSettings,
	const FString& PartyMatchKey)
{
	TMap<FString, FEIKAttribute> Result = SessionSettings;
	AddPartyMatchKeySetting(Result, PartyMatchKey);
	return Result;
}

bool UEIK_MatchmakingLibrary::FindSessionByPartyMatchKey(
	const TArray<FSessionFindStruct>& SessionResults,
	const FString& ExpectedPartyMatchKey,
	FSessionFindStruct& MatchedSession)
{
	const FString ExpectedKey = NormalizeMatchKey(ExpectedPartyMatchKey);
	if (ExpectedKey.IsEmpty())
	{
		MatchedSession = FSessionFindStruct();
		return false;
	}

	for (const FSessionFindStruct& SessionResult : SessionResults)
	{
		if (DoesSettingMatch(SessionResult.SessionSettings, PartyMatchKeySettingName, ExpectedKey) ||
			DoesSettingMatch(SessionResult.SessionSettings, TEXT("SETTING_SESSION_ID_OVERRIDE"), ExpectedKey) ||
			NormalizeMatchKey(SessionResult.SessionName) == ExpectedKey)
		{
			MatchedSession = SessionResult;
			return true;
		}
	}

	MatchedSession = FSessionFindStruct();
	return false;
}

FString UEIK_MatchmakingLibrary::GetSessionStringSetting(
	const FSessionFindStruct& SessionResult,
	const FString& SettingName,
	bool& bFound)
{
	if (const FEIKAttribute* Attribute = SessionResult.SessionSettings.Find(SettingName))
	{
		bFound = true;
		return AttributeToString(*Attribute);
	}

	bFound = false;
	return FString();
}

void UEIK_MatchmakingLibrary::LogPartyMatchSearchResults(
	const TArray<FSessionFindStruct>& SessionResults,
	const FString& ExpectedPartyMatchKey)
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[EIKMatchmaking] Search results: ExpectedPartyMatchKey='%s', Count=%d"),
		*ExpectedPartyMatchKey,
		SessionResults.Num());

	for (int32 Index = 0; Index < SessionResults.Num(); ++Index)
	{
		const FSessionFindStruct& SessionResult = SessionResults[Index];
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[EIKMatchmaking] Result[%d]: SessionName='%s', PartyMatchKey='%s', SessionIdOverride='%s', Players=%d/%d, Dedicated=%s"),
			Index,
			*SessionResult.SessionName,
			*FindStringSettingOrEmpty(SessionResult, PartyMatchKeySettingName),
			*FindStringSettingOrEmpty(SessionResult, TEXT("SETTING_SESSION_ID_OVERRIDE")),
			SessionResult.CurrentNumberOfPlayers,
			SessionResult.MaxNumberOfPlayers,
			SessionResult.bIsDedicatedServer ? TEXT("true") : TEXT("false"));
	}
}
