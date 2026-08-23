// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystem/Online/CSOnlineTypes.h"

class UCSOnlineSettings;

/**
 * The adapter interface every online backend implements.
 *
 * PLAIN C++, NOT A UINTERFACE, and there is no .generated.h in this file on purpose.
 * It is consumed only from C++, needs TSharedPtr lifetime, and must be constructible
 * before any UObject exists (FCSOnlineBackendRegistry is a lazy function-local static).
 * A UObject interface would force GC rooting and buy nothing -- the Blueprint surface
 * lives entirely on UCSOnlineSessionSubsystem.
 *
 * No #if guards here either. The per-backend guards live in the concrete adapters and in
 * FCSOnlineBackendRegistry, which is the only file in the codebase that names both.
 *
 * Held as TSharedPtr<ICSOnlineBackend> / TSharedRef<ICSOnlineBackend>; hence the virtual
 * destructor below.
 */
class CHRONOSPACE_API ICSOnlineBackend
{
public:
	virtual ~ICSOnlineBackend() = default;

	// --- identity of the adapter -------------------------------------
	virtual ECSOnlineBackend GetBackendId() const = 0;
	virtual FName            GetSubsystemName() const = 0;
	virtual FText            GetDisplayName() const = 0;

	/**
	 * Module loadable, OSS instance creatable, prerequisites met.
	 * Must be cheap and safe to call at any time, including before login.
	 * Must NOT require the user to be logged in -- the facade drives login.
	 */
	virtual bool IsAvailable() const = 0;

	/**
	 * Human-readable reason IsAvailable() is false. Empty when available.
	 *
	 * This exists because both real backends fail SILENTLY by default:
	 * LoadDefaultSubsystem falls back to NULL with the failure logged only at
	 * LogOnline Verbose (OnlineSubsystemModule.cpp:216-221), and
	 * FOnlineSubsystemSteam::Init() returns false with nothing but
	 * "Could not set up the steam environment!" when SteamDevAppId is missing.
	 * Surfacing the reason into the log/UI is the difference between a five-minute
	 * and a five-hour diagnosis.
	 */
	virtual FString GetUnavailableReason() const = 0;

	// --- transport ----------------------------------------------------
	/** Class path fed to FCSNetDriverSwitcher for the GameNetDriver definition. */
	virtual FString GetNetDriverClassName() const = 0;

	// --- THE invariant ------------------------------------------------
	/**
	 * Single source of truth for lobbies-vs-sessions. Read by BOTH
	 * FillCreateSettings and FillSearchSettings. Never branch on anything
	 * else for this decision.
	 *
	 * The create side and the find side must agree or joins fail silently and
	 * confusingly: EIK routes JoinSession on the flag carried by the search result
	 * (OnlineSessionEOS.cpp:2661), and Steam hard-fails CreateSession/JoinSession
	 * when bUsesPresence != bUseLobbiesIfAvailable
	 * (OnlineSessionInterfaceSteam.cpp:229-236, :861).
	 */
	virtual bool UsesLobbies() const = 0;

	// --- identity -----------------------------------------------------
	/** False when the backend is already authenticated by the platform (Steam, Null). */
	virtual bool NeedsExplicitLogin() const = 0;

	/** Returns false when the backend needs no credentials; OutCreds is then untouched. */
	virtual bool BuildLoginCredentials(int32 LocalUserNum,
	                                   FOnlineAccountCredentials& OutCreds) const = 0;

	// --- session shaping ---------------------------------------------
	virtual void FillCreateSettings(FOnlineSessionSettings& InOutSettings,
	                                const FCSHostSessionParams& Params,
	                                const UCSOnlineSettings& Config) const = 0;

	virtual void FillSearchSettings(FOnlineSessionSearch& InOutSearch,
	                                const FCSFindSessionsParams& Params,
	                                const UCSOnlineSettings& Config) const = 0;

	/** Client-side filter applied to every raw result before it becomes a FCSSessionSearchResult. */
	virtual bool ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result,
	                                      const FCSFindSessionsParams& Params,
	                                      const UCSOnlineSettings& Config) const = 0;

	// --- travel-URL hooks (default no-op) ----------------------------
	virtual void DecorateListenURL(FString& InOutURL) const {}
	virtual void DecorateClientTravelURL(FString& InOutURL) const {}

	// --- dedicated-server hook, default = unsupported -----------------
	/**
	 * Present so the future dedicated track has a seam. Returning false
	 * makes UCSOnlineSessionSubsystem refuse to host as a dedicated server
	 * on this backend with a clear message rather than half-working.
	 */
	virtual bool BuildDedicatedServerCredentials(FOnlineAccountCredentials& OutCreds) const { return false; }

	// --- non-virtual conveniences (implemented in the .cpp) ----------
	// GetSubsystem() resolves IOnlineSubsystem::Get(GetSubsystemName()). Never call
	// IOnlineSubsystem::Get() with no argument anywhere in this layer: the whole design
	// depends on addressing backends explicitly so DefaultPlatformService is irrelevant
	// to us. The interface getters null-check the subsystem and return an empty
	// TSharedPtr rather than dereferencing -- that is the specific fix for the
	// unconditional dereferences at CSEIKSubsystem.cpp:122-124, :147-149, :200-202,
	// :251-253, which reliably crash when the backend is not running.
	IOnlineSubsystem*  GetSubsystem() const;
	IOnlineSessionPtr  GetSessionInterface() const;
	IOnlineIdentityPtr GetIdentityInterface() const;
	bool               IsLoggedIn(int32 LocalUserNum = 0) const;

protected:
	// Shared helpers every adapter uses, so the advertised-key spelling
	// cannot drift between backends.

	/**
	 * Applies only what is genuinely identical across backends. Advertisement type is
	 * always ViaOnlineService or higher: anything lower never reaches the backend at all
	 * (Steam drops it at OnlineSessionAsyncLobbySteam.cpp:130, EIK only forwards
	 * >= ViaOnlineService at OnlineSessionEOS.cpp:1313).
	 */
	static void ApplyCommonCreateSettings(FOnlineSessionSettings& InOut,
	                                      const FCSHostSessionParams& Params,
	                                      const UCSOnlineSettings& Config);

	/** Reads UCSOnlineSettings::GameIdTagKey back off the result and string-compares. True when filtering is off. */
	static bool MatchesGameIdTag(const FOnlineSessionSearchResult& Result,
	                             const UCSOnlineSettings& Config);

	/**
	 * Compares Result.Session.SessionSettings.BuildUniqueId against GetBuildUniqueId().
	 * Steam already discards mismatches server-side
	 * (OnlineSessionAsyncLobbySteam.cpp:405-413) and the EIK Sessions path filters by
	 * bucket, but the EIK *lobby* path filters by neither -- so this must exist client-side.
	 */
	static bool MatchesBuildId(const FOnlineSessionSearchResult& Result);
};
