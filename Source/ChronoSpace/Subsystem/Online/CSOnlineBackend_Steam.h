// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemNames.h"
#include "Subsystem/Online/CSOnlineBackend.h"

// THE GUARD WRAPS EVERYTHING BELOW, DELIBERATELY.
// With CS_WITH_STEAM=0 this header declares nothing and FCSOnlineBackend_Steam does not
// exist as a type. Only CSOnlineBackendRegistry.cpp may include this file, and only under
// the same guard.
//
// CS_WITH_STEAM is emitted as a paired 1/0 PublicDefinition by ChronoSpace.Build.cs, so
// this #if is always well-defined and -Wundef / MSVC C4668 never fire.
//
// This is a POLICY guard, not a link guard. STEAM_SUBSYSTEM is declared by the GENERIC
// OnlineSubsystem plugin (OnlineSubsystemNames.h:53-54), not by OnlineSubsystemSteam, and
// IOnlineSubsystem::Get(FName) resolves the DLL by string concatenation at runtime
// (OnlineSubsystemModule.cpp:22-42, :382). This adapter therefore has no link-time
// coupling to OnlineSubsystemSteam whatsoever -- the guard exists only so an adapter for a
// backend that cannot work in this configuration is not compiled at all.
#if CS_WITH_STEAM

/**
 * Steam adapter.
 *
 * Every failure mode on this backend is silent by default, which is why
 * GetUnavailableReason() carries real diagnostic text rather than a boolean:
 * FOnlineSubsystemSteam::Init() returns false with nothing but "Could not set up the steam
 * environment!" when SteamDevAppId is missing, and AreSteamDllsLoaded gates factory
 * registration at OnlineSubsystemModuleSteam.cpp:82 without surfacing anything either.
 */
class FCSOnlineBackend_Steam final : public ICSOnlineBackend
{
public:
	virtual ECSOnlineBackend GetBackendId() const override { return ECSOnlineBackend::Steam; }

	/** UCSOnlineSettings::SteamSubsystemName, defaulting to STEAM_SUBSYSTEM. */
	virtual FName   GetSubsystemName() const override;
	virtual FText   GetDisplayName() const override;

	/** IsSteamUsableInThisProcess() && subsystem && session interface && identity interface. */
	virtual bool    IsAvailable() const override;

	/**
	 * Distinguishes the four failure modes, all of which are silent by default: editor
	 * process, Steam OSS not created (client not running / bEnabled=false / missing
	 * SteamDevAppId / steam_api64.dll not loadable), -nosteam on the command line, and
	 * missing interfaces.
	 */
	virtual FString GetUnavailableReason() const override;

	/**
	 * UCSOnlineSettings::SteamNetDriverClassName, i.e. /Script/SteamSockets.SteamSocketsNetDriver.
	 * NOT /Script/OnlineSubsystemSteam.SteamNetDriver: that class is dead in UE 5.8 (the
	 * classes moved to SocketSubsystemSteamIP with no CoreRedirect, while BaseEngine.ini:2521
	 * still ships the stale section), so it resolves to nothing and silently falls back to
	 * IpNetDriver.
	 */
	virtual FString GetNetDriverClassName() const override;

	/**
	 * Unconditionally true. The implementation logs an Error rather than honouring
	 * UCSOnlineSettings::bSteamUseLobbies=false: the non-lobby Steam path is the server
	 * browser, which needs the Steam game-server API (bInitServerOnClient=true or a
	 * dedicated server) that this project does not run.
	 *
	 * This one value drives both create and find. FOnlineSessionSteam::CreateSession fires
	 * OnCreateSessionComplete(false) and returns immediately when
	 * bUsesPresence != bUseLobbiesIfAvailable (OnlineSessionInterfaceSteam.cpp:229-236),
	 * and JoinSession repeats the check at :861.
	 */
	virtual bool    UsesLobbies() const override { return true; }

	/**
	 * False. FOnlineIdentitySteam::Login ignores credentials entirely and just checks
	 * BLoggedOn() (OnlineIdentityInterfaceSteam.cpp:33), and GetUniquePlayerId(0) is valid
	 * before any Login call (:113).
	 */
	virtual bool    NeedsExplicitLogin() const override { return false; }

	/** Returns false; Steam needs no credentials. OutCreds is left untouched. */
	virtual bool    BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const override;

	virtual void    FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const override;
	virtual void    FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;
	virtual bool    ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;

private:
	/**
	 * False inside the editor process. FOnlineSubsystemSteam::IsEnabled() is
	 * "#if UE_EDITOR bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();"
	 * (OnlineSubsystemSteam.cpp:479-484), and both socket-subsystem modules log
	 * "Disabled for editor process.", so PIE-in-editor can never exercise Steam.
	 * Reporting unavailable up front beats failing deep inside CreateSession.
	 */
	static bool IsSteamUsableInThisProcess();
};

#endif // CS_WITH_STEAM
