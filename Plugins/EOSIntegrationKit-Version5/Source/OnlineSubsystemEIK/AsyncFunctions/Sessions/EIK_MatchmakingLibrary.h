// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OnlineSubsystemEIK/Subsystem/EIK_Subsystem.h"
#include "EIK_MatchmakingLibrary.generated.h"

UCLASS()
class ONLINESUBSYSTEMEIK_API UEIK_MatchmakingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "EOS Integration Kit || Sessions")
	static FEIKAttribute MakeEIKStringAttribute(const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "EOS Integration Kit || Sessions")
	static void AddPartyMatchKeySetting(
		UPARAM(ref) TMap<FString, FEIKAttribute>& SessionSettings,
		const FString& PartyMatchKey);

	UFUNCTION(BlueprintPure, Category = "EOS Integration Kit || Sessions")
	static TMap<FString, FEIKAttribute> WithPartyMatchKeySetting(
		const TMap<FString, FEIKAttribute>& SessionSettings,
		const FString& PartyMatchKey);

	UFUNCTION(BlueprintPure, Category = "EOS Integration Kit || Sessions")
	static bool FindSessionByPartyMatchKey(
		const TArray<FSessionFindStruct>& SessionResults,
		const FString& ExpectedPartyMatchKey,
		FSessionFindStruct& MatchedSession);

	UFUNCTION(BlueprintPure, Category = "EOS Integration Kit || Sessions")
	static FString GetSessionStringSetting(
		const FSessionFindStruct& SessionResult,
		const FString& SettingName,
		bool& bFound);

	UFUNCTION(BlueprintCallable, Category = "EOS Integration Kit || Sessions")
	static void LogPartyMatchSearchResults(
		const TArray<FSessionFindStruct>& SessionResults,
		const FString& ExpectedPartyMatchKey);
};
