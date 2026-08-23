// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/Online/CSOnlineBackend_Null.h"

#include "OnlineSubsystemNames.h"
#include "Online/OnlineSessionNames.h"
#include "Settings/CSOnlineSettings.h"
#include "ChronoSpace.h"

/**
 * The always-compiled adapter over NULL_SUBSYSTEM.
 *
 * NO #if ANYWHERE IN THIS FILE, deliberately. This adapter is the terminator of
 * FCSOnlineBackendRegistry::FindFirstAvailable -- which returns a TSharedRef, not a
 * TSharedPtr -- so it has to exist in every build configuration or the facade grows a
 * null-backend path that nothing exercises.
 *
 * This adapter binds NO OSS delegates and holds no state. Every delegate on the online path
 * is owned by UCSOnlineSessionSubsystem, which clears its handles both on completion and
 * again in Deinitialize(). Do not start binding delegates here without adding a teardown
 * path: the registry keeps one instance of this adapter alive for the whole process, far
 * longer than any single operation or GameInstance.
 */

FName FCSOnlineBackend_Null::GetSubsystemName() const
{
	// NULL_SUBSYSTEM is an extern const FName declared by the *generic* OnlineSubsystem
	// plugin (OnlineSubsystemNames.h:21-22), which ChronoSpace.Build.cs already depends on.
	// Naming it here therefore adds no dependency on the OnlineSubsystemNull module itself --
	// IOnlineSubsystem::Get() loads that by string concatenation at runtime.
	return NULL_SUBSYSTEM;
}

FText FCSOnlineBackend_Null::GetDisplayName() const
{
	return NSLOCTEXT("ChronoSpace", "Backend_Null", "Offline / LAN");
}

bool FCSOnlineBackend_Null::IsAvailable() const
{
	// ICSOnlineBackend::GetSubsystem() resolves IOnlineSubsystem::Get(NULL_SUBSYSTEM) and
	// returns null instead of dereferencing. Never IOnlineSubsystem::Get() with no argument
	// anywhere in this layer: the design addresses backends explicitly so that
	// [OnlineSubsystem] DefaultPlatformService cannot silently redirect us.
	//
	// A non-null subsystem is the whole test. The Null OSS is always constructible -- it has
	// no external client, no credentials and no network prerequisite -- so there is nothing
	// further to probe. Availability here does NOT imply the session/identity interfaces are
	// valid; every consumer goes through ICSOnlineBackend::GetSessionInterface() /
	// GetIdentityInterface(), which null-check and hand back an empty TSharedPtr.
	return GetSubsystem() != nullptr;
}

FString FCSOnlineBackend_Null::GetUnavailableReason() const
{
	if (IsAvailable())
	{
		return FString();
	}

	// Reaching here is close to pathological: OnlineSubsystemNull is "EnabledByDefault": true
	// in its .uplugin and IOnlineSubsystem::IsEnabled defaults to true when no config key
	// exists at all (OnlineSubsystem.cpp:494-496). It is still worth naming the three ways it
	// can happen, because the engine reports the creation failure only at LogOnline Verbose
	// (OnlineSubsystemModule.cpp:216-221) -- silence that turns a five-minute diagnosis into a
	// five-hour one. The command-line form is "-noNULL": IsEnabled() consults
	// CheckSubsystemDisabledByCommandLine, which does FParse::Param(..., "no%s") against the
	// subsystem name (OnlineSubsystem.cpp:411), and that override exists in every
	// non-shipping build.
	return TEXT("The NULL online subsystem could not be created. Check that the OnlineSubsystemNull plugin is enabled, that [OnlineSubsystemNull] bEnabled is not False in DefaultEngine.ini, and that -noNULL was not passed on the command line.");
}

FString FCSOnlineBackend_Null::GetNetDriverClassName() const
{
	// Plain IpNetDriver. This is also DriverClassNameFallback for the other backends, so the
	// deliberate Null path and the engine's degraded path (CreateNetDriver_Local falls back
	// when the class fails to load or its CDO->IsAvailable() is false,
	// UnrealEngine.cpp:15020-15024) converge on exactly the same transport.
	return UCSOnlineSettings::Get().FallbackNetDriverClassName;
}

bool FCSOnlineBackend_Null::BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const
{
	// NeedsExplicitLogin() is false for this backend: the Null identity interface fabricates a
	// local account with no credentials at all. Returning false leaves OutCreds untouched, per
	// the ICSOnlineBackend contract, and tells the facade to skip the login step entirely
	// rather than issue a Login() that would only ever be a no-op.
	return false;
}

void FCSOnlineBackend_Null::FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const
{
	ApplyCommonCreateSettings(InOutSettings, Params, Config);

	// Forced true rather than taken from Params.bIsLANMatch. LAN is the only discovery the
	// Null OSS has: FOnlineSessionNull::FindSessions routes unconditionally to
	// FindLANSession (OnlineSessionInterfaceNull.cpp:530) and NeedsToAdvertise only hosts the
	// beacon for a bIsLANMatch session (:288-296). A host created with bIsLANMatch false would
	// succeed and then be invisible to everyone.
	InOutSettings.bIsLANMatch = true;

	// LOAD-BEARING, and essentially only on this backend. FOnlineSessionNull::CreateSession
	// calls UpdateLANStatus (OnlineSessionInterfaceNull.cpp:226), which calls NeedsToAdvertise
	// (:288) -- and that function opens with "Session.SessionSettings.bShouldAdvertise &&
	// IsHost(Session) && ...". With bShouldAdvertise false the LAN beacon is never hosted,
	// CreateSession still reports success, and the session is simply undiscoverable with
	// nothing in the log to say why. Do not drop this line because "the other adapters do not
	// set it" -- they genuinely do not need it, and this one genuinely does.
	InOutSettings.bShouldAdvertise = true;

	// Written as a matched pair, consistent with UsesLobbies() == false. The Null OSS does not
	// enforce the Steam equality rule (CreateSession/JoinSession hard-fail there when
	// bUsesPresence != bUseLobbiesIfAvailable, OnlineSessionInterfaceSteam.cpp:229-236, :861),
	// so nothing here breaks today -- but keeping them in lockstep means retargeting this
	// adapter later cannot quietly violate the invariant.
	InOutSettings.bUsesPresence = false;
	InOutSettings.bUseLobbiesIfAvailable = false;

	UE_LOG(LogCS, Verbose, TEXT("[Online][Null] Create settings: LAN=1 Advertise=1 Lobbies=0 PublicConnections=%d JoinInProgress=%d BuildUniqueId=%d"),
		InOutSettings.NumPublicConnections,
		InOutSettings.bAllowJoinInProgress ? 1 : 0,
		InOutSettings.BuildUniqueId);
}

void FCSOnlineBackend_Null::FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	// Forced true for the same reason FillCreateSettings forces bIsLANMatch. Params.bLANQuery
	// is ignored rather than honoured here so that a caller who left it false still finds LAN
	// hosts instead of silently finding nothing. (The Null session interface itself never
	// reads this flag -- FindSessions goes straight to FindLANSession,
	// OnlineSessionInterfaceNull.cpp:530 -- but the search object is also read by the facade
	// and by shared code, so it must describe what the search actually is.)
	InOutSearch.bIsLanQuery = true;

	// FOnlineSessionSearch defaults MaxSearchResults to 1 (OnlineSessionSettings.h:600), and a
	// zero or negative value arriving from a Blueprint that bypassed the ClampMin=1 meta would
	// return nothing at all. Clamp rather than trust.
	InOutSearch.MaxSearchResults = FMath::Max(1, Params.MaxResults);

	// No query keys, on purpose. There is no service-side query engine on this backend to
	// evaluate QuerySettings against: the LAN beacon answers with the host's whole
	// FOnlineSessionSettings and the client filters locally. Anything written into
	// QuerySettings here would be dead weight that only looks like it is doing something.
	// Discrimination happens in ShouldAcceptSearchResult instead.

	UE_LOG(LogCS, Verbose, TEXT("[Online][Null] Search settings: LANQuery=1 MaxResults=%d FilterByBuildId=%d"),
		InOutSearch.MaxSearchResults,
		Params.bFilterByBuildId ? 1 : 0);
}

bool FCSOnlineBackend_Null::ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	// BuildUniqueId only. Params.bFilterByGameIdTag is deliberately not consulted on this
	// backend: the advertised game-id tag is a discriminator for an online service's own
	// query, and there is no service here to have run one.
	//
	// The build-id check is not a formality though, it is the only discriminator this path
	// has. The LAN beacon's packet header carries a game id that defaults to
	// LAN_UNIQUE_ID == 9999 (LANBeacon.h:169, :241) unless [LANSession] LanGameUniqueId is set
	// in DefaultEngine.ini -- i.e. it is shared with every other Unreal project on the subnet
	// that also left it at the default, so foreign hosts really can answer our query.
	// BuildUniqueId survives the trip: FOnlineSessionNull::AppendSessionSettingsToPacket
	// serialises it into the host response (OnlineSessionInterfaceNull.cpp:1032), so
	// MatchesBuildId has real data to compare against GetBuildUniqueId().
	//
	// Params.bFilterByBuildId is honoured here rather than inside MatchesBuildId, because that
	// shared helper takes only the result -- the toggle has to be applied by the caller or it
	// would be silently dead on this backend while working on the others.
	return !Params.bFilterByBuildId || MatchesBuildId(Result);
}
