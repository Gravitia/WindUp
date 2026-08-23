// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystem/Online/CSOnlineBackend.h"

/**
 * Always-compiled fallback adapter over NULL_SUBSYSTEM.
 *
 * NO #if GUARD ON THIS FILE, DELIBERATELY -- unlike the EIK and Steam adapters this one
 * always compiles. It is what makes "no backend" unrepresentable in the facade:
 * FCSOnlineBackendRegistry::FindFirstAvailable returns a TSharedRef and terminates on this
 * adapter, so UCSOnlineSessionSubsystem never has a null-backend path to get wrong.
 *
 * It also gives PIE and LAN a route that actually works. Steam OSS is force-disabled
 * inside the editor process (FOnlineSubsystemSteam::IsEnabled is
 * "bEnableSteam = IsRunningDedicatedServer() || IsRunningGame()" under UE_EDITOR,
 * OnlineSubsystemSteam.cpp:479-484) and EIK P2P is awkward there.
 */
class FCSOnlineBackend_Null final : public ICSOnlineBackend
{
public:
	virtual ECSOnlineBackend GetBackendId() const override { return ECSOnlineBackend::Null; }

	/** NULL_SUBSYSTEM, declared by the generic OnlineSubsystem plugin (OnlineSubsystemNames.h:21-22). */
	virtual FName   GetSubsystemName() const override;
	virtual FText   GetDisplayName() const override;

	/** The Null OSS is always constructible, so this is just a non-null subsystem check. */
	virtual bool    IsAvailable() const override;
	virtual FString GetUnavailableReason() const override;

	/** UCSOnlineSettings::FallbackNetDriverClassName, i.e. plain IpNetDriver. */
	virtual FString GetNetDriverClassName() const override;

	/** The Null OSS has no lobby concept. Constant, so create and find can never disagree here. */
	virtual bool    UsesLobbies() const override { return false; }

	virtual bool    NeedsExplicitLogin() const override { return false; }
	virtual bool    BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const override;

	/**
	 * ApplyCommonCreateSettings, then forces bIsLANMatch/bShouldAdvertise true and
	 * bUsesPresence/bUseLobbiesIfAvailable false. bShouldAdvertise genuinely matters here
	 * and essentially only here: CreateLANSession is the one path the engine actually
	 * reads it on.
	 */
	virtual void    FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const override;

	/** LAN query, MaxSearchResults from Params, no query keys at all. */
	virtual void    FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;

	/** MatchesBuildId only -- there is no online service to have advertised a game-id tag. */
	virtual bool    ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const override;
};
