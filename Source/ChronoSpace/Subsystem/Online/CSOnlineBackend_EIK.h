// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystem/Online/CSOnlineBackend.h"

// THE GUARD WRAPS EVERYTHING BELOW, DELIBERATELY.
// With CS_WITH_EIK=0 this header declares nothing and FCSOnlineBackend_EIK does not
// exist as a type. Only CSOnlineBackendRegistry.cpp may include this file, and only
// under the same guard. That is what makes the eventual Steam-only cutover a deletion of
// CSOnlineBackend_EIK.{h,cpp} plus one line of .uproject JSON, with no change anywhere in
// UCSOnlineSessionSubsystem.
//
// CS_WITH_EIK is emitted as a paired 1/0 PublicDefinition by ChronoSpace.Build.cs, so
// this #if is always well-defined and -Wundef / MSVC C4668 never fire.
//
// Note this is a POLICY guard, not a link guard: IOnlineSubsystem::Get(FName) resolves
// the DLL by string concatenation "OnlineSubsystem" + Name through FModuleManager at
// runtime (OnlineSubsystemModule.cpp:22-42, :382), so nothing here links against the EIK
// plugin and the module needs no extra dependency.
#if CS_WITH_EIK

/**
 * EIK / EOS adapter.
 *
 * IsAvailable() checks subsystem, session interface and identity interface only. It
 * deliberately does NOT require the user to be logged in -- the facade drives login.
 */
class FCSOnlineBackend_EIK final : public ICSOnlineBackend
{
public:
	virtual ECSOnlineBackend GetBackendId() const override { return ECSOnlineBackend::EIK; }

	/**
	 * UCSOnlineSettings::EIKSubsystemName, defaulting to FName(TEXT("EIK")).
	 * There is genuinely no macro to use: grepping the plugin for EIK_SUBSYSTEM /
	 * EOS_SUBSYSTEM returns nothing, and OnlineSubsystemModuleEIK.cpp:66 registers a bare
	 * "EIK" string literal. Hence the name is surfaced through settings rather than
	 * hardcoded here.
	 */
	virtual FName   GetSubsystemName() const override;
	virtual FText   GetDisplayName() const override;
	virtual bool    IsAvailable() const override;

	/**
	 * Distinguishes "OnlineSubsystemEIK module not loaded", "[OnlineSubsystemEIK]
	 * bEnabled=false", and "session/identity interface missing". All three are silent by
	 * default -- LoadDefaultSubsystem falls back to NULL and logs the real cause only at
	 * LogOnline Verbose (OnlineSubsystemModule.cpp:216-221).
	 */
	virtual FString GetUnavailableReason() const override;

	/**
	 * UCSOnlineSettings::EIKNetDriverClassName.
	 * The resolved connect string is only usable while this driver is live with
	 * bIsUsingP2PSockets=true; with that flag false CreateEOSSession silently advertises
	 * 127.0.0.1 (OnlineSessionEOS.cpp:1512), so every join fails at travel time with no
	 * error at create time.
	 */
	virtual FString GetNetDriverClassName() const override;

	/** UCSOnlineSettings::bEIKUseLobbies. Read by BOTH FillCreateSettings and FillSearchSettings. */
	virtual bool    UsesLobbies() const override;

	virtual bool    NeedsExplicitLogin() const override { return true; }

	/**
	 * Device-id credentials for LocalUserNum 0 only. EIK hardcodes LocalUserNum 0 on the
	 * connect-interface path (UserManagerEOS.cpp:660, :691) and the first-run device-id
	 * fallback indexes LocalUserNumToLastLoginCredentials[0] (:1077), so anything else
	 * silently will not work; the implementation logs a Warning and returns false.
	 *
	 * The id comes from UCSLoginIdLibrary, not FGuid::NewGuid(): CSEIKSubsystem.cpp:58
	 * mints a fresh device id every launch, so the EOS account was never stable across
	 * sessions. The library persists to GGameUserSettingsIni, honours -LoginId= and clamps
	 * to 32 chars, and it is what the live Blueprints already use.
	 */
	virtual bool    BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const override;

	virtual void    FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const override;
	virtual void    FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;
	virtual bool    ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;

private:
	/**
	 * Returns the exact literal TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN").
	 *
	 * THIS IS THE SINGLE MOST FRAGILE STRING IN THE SYSTEM. It is compared with plain
	 * FString == at UserManagerEOS.cpp:1077 to decide whether to auto-CreateDeviceID on
	 * first launch. ANY deviation -- including the fully-qualified
	 * EEIK_EExternalCredentialType::EIK_ECT_... form that EIK's own Blueprint node
	 * produces at EIK_Login_AsyncFunction.cpp:72 -- makes first launch on a fresh machine
	 * return EOS_NotFound and fail instead of provisioning the device id.
	 * Do not reconstruct it from an enum, and do not "tidy" the prefix.
	 */
	static const TCHAR* GetDeviceIdCredentialType();
};

#endif // CS_WITH_EIK
