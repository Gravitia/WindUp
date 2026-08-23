// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/Online/CSOnlineBackend_Steam.h"

// THE GUARD WRAPS EVERYTHING BELOW, matching CSOnlineBackend_Steam.h.
// With CS_WITH_STEAM=0 this translation unit is empty and FCSOnlineBackend_Steam does not
// exist as a type. The include above is deliberately OUTSIDE the guard: the header defines
// CS_WITH_STEAM's meaning for this file and is itself fully guarded, so including it
// unconditionally costs nothing and keeps IWYU / static analysis honest.
#if CS_WITH_STEAM

#include "ChronoSpace.h"
#include "Settings/CSOnlineSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "Online/OnlineSessionNames.h"

namespace
{
	/**
	 * UsesLobbies() is an inline "return true" in the header, so there is no .cpp body to
	 * host this check. It runs instead at the two points where bSteamUseLobbies=false would
	 * otherwise have taken effect -- create and find -- which is also where anyone chasing
	 * "why is it still using lobbies?" will be reading.
	 *
	 * The setting is honoured by being refused, not by being obeyed: the non-lobby Steam path
	 * is the server browser (FOnlineAsyncTaskSteamFindServers), which needs the Steam
	 * game-server API -- bInitServerOnClient=true or an actual dedicated server -- that this
	 * project does not run.
	 */
	void CSSteam_WarnIfLobbiesDisabled(const UCSOnlineSettings& Config)
	{
		if (!Config.bSteamUseLobbies)
		{
			UE_LOG(LogCS, Error, TEXT("[Online] bSteamUseLobbies=false is not supported; the non-lobby Steam path is the server browser and needs the Steam game-server API (bInitServerOnClient=true or a dedicated server), which this project does not run. Forcing lobbies."));
		}
	}
}

// -----------------------------------------------------------------------------------
// Identity of the adapter
// -----------------------------------------------------------------------------------

FName FCSOnlineBackend_Steam::GetSubsystemName() const
{
	const FName Configured = UCSOnlineSettings::Get().SteamSubsystemName;

	// NAME_None must never reach IOnlineSubsystem::Get(). FOnlineSubsystemModule::
	// ParseOnlineSubsystemName substitutes DefaultPlatformService whenever the requested name
	// is None (OnlineSubsystemModule.cpp:319), so a cleared SteamSubsystemName would silently
	// hand this adapter the EIK subsystem instead of failing. Addressing every backend
	// explicitly by name is the entire premise of this layer.
	return Configured.IsNone() ? STEAM_SUBSYSTEM : Configured;
}

FText FCSOnlineBackend_Steam::GetDisplayName() const
{
	return NSLOCTEXT("ChronoSpace", "Backend_Steam", "Steam");
}

// -----------------------------------------------------------------------------------
// Availability
// -----------------------------------------------------------------------------------

bool FCSOnlineBackend_Steam::IsSteamUsableInThisProcess()
{
#if WITH_EDITOR
	// FOnlineSubsystemSteam::IsEnabled() is
	//     #if UE_EDITOR bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();
	// (OnlineSubsystemSteam.cpp:479-484), and both Steam socket-subsystem modules skip
	// startup with "Disabled for editor process." PIE-in-editor therefore can NEVER exercise
	// the Steam path, no matter what DefaultEngine.ini says. Reporting that up front is the
	// difference between a clear "use Standalone" message and a CreateSession that fails
	// several layers down for no visible reason.
	//
	// IsRunningGame() is true in the -game process that Play > Standalone / Launch spawns,
	// which is exactly the case that must stay usable.
	if (!IsRunningDedicatedServer() && !IsRunningGame())
	{
		return false;
	}
#endif
	return true;
}

bool FCSOnlineBackend_Steam::IsAvailable() const
{
	// Deliberately does NOT test login status. ICSOnlineBackend::IsAvailable() is contracted
	// to be cheap and safe to call at any time including before login, and the facade drives
	// login; FCSOnlineBackendRegistry::FindFirstAvailable calls this on every candidate during
	// backend resolution, long before anyone has signed in.
	//
	// The case this therefore does not catch is a Steam client running in OFFLINE mode: the
	// subsystem and both interfaces exist, but SteamUser()->BLoggedOn() is false, so
	// FOnlineIdentitySteam::GetLoginStatus returns NotLoggedIn
	// (OnlineIdentityInterfaceSteam.cpp:102-110) and FOnlineAsyncTaskSteamFindLobbiesBase
	// never issues RequestLobbyList (OnlineSessionAsyncLobbySteam.cpp:955-970) -- a find that
	// "succeeds" with zero results. That is a login failure, not an availability failure, and
	// belongs to the facade's login step, which can surface and retry it. ICSOnlineBackend
	// ::IsLoggedIn() is the check to use there.
	return IsSteamUsableInThisProcess()
		&& GetSubsystem() != nullptr
		&& GetSessionInterface().IsValid()
		&& GetIdentityInterface().IsValid();
}

FString FCSOnlineBackend_Steam::GetUnavailableReason() const
{
	// Every branch below is a failure mode that is SILENT by default. That is the whole
	// reason this returns diagnostic text instead of a bool.
	if (!IsSteamUsableInThisProcess())
	{
		return TEXT("Steam is disabled inside the editor process; use Standalone or a packaged build. ")
			   TEXT("(FOnlineSubsystemSteam::IsEnabled() forces this under UE_EDITOR, OnlineSubsystemSteam.cpp:479-484.)");
	}

	// Checked BEFORE the subsystem-null branch, because -nosteam is what causes the subsystem
	// to be null (OnlineSubsystemModuleSteam.cpp:73-75). Reporting "not created" here would
	// send the reader hunting through ini settings that are all correct.
	if (FParse::Param(FCommandLine::Get(), TEXT("nosteam")))
	{
		return TEXT("Steam was disabled by the command line (-nosteam). See OnlineSubsystemModuleSteam.cpp:73-75.");
	}

	const FName SubsystemName = GetSubsystemName();

	if (GetSubsystem() == nullptr)
	{
		return FString::Printf(
			TEXT("Steam OSS '%s' was not created. Check, in order: the Steam client is running and logged in; ")
			TEXT("[OnlineSubsystemSteam] bEnabled=true; SteamDevAppId is set (a missing SteamDevAppId makes ")
			TEXT("FOnlineSubsystemSteam::Init() return false outright, logging only \"Could not set up the steam environment!\"); ")
			TEXT("steam_api64.dll is loadable (AreSteamDllsLoaded gates factory registration at OnlineSubsystemModuleSteam.cpp:82). ")
			TEXT("Note that the OnlineSubsystemSteam plugin must be enabled in ChronoSpace.uproject -- listing only the module ")
			TEXT("in Build.cs leaves its Config unlayered and steam_api64.dll unstaged, which fails exactly like this."),
			*SubsystemName.ToString());
	}

	const bool bHasSession = GetSessionInterface().IsValid();
	const bool bHasIdentity = GetIdentityInterface().IsValid();
	if (!bHasSession || !bHasIdentity)
	{
		return FString::Printf(TEXT("Steam OSS '%s' exists but is missing interfaces (session=%s, identity=%s)."),
			*SubsystemName.ToString(),
			bHasSession ? TEXT("ok") : TEXT("MISSING"),
			bHasIdentity ? TEXT("ok") : TEXT("MISSING"));
	}

	// Available.
	return FString();
}

// -----------------------------------------------------------------------------------
// Transport
// -----------------------------------------------------------------------------------

FString FCSOnlineBackend_Steam::GetNetDriverClassName() const
{
	// Defaults to /Script/SteamSockets.SteamSocketsNetDriver.
	//
	// NOT /Script/OnlineSubsystemSteam.SteamNetDriver: that class no longer exists in UE 5.8
	// (it moved to the SocketSubsystemSteamIP plugin, with NO CoreRedirect, while
	// BaseEngine.ini:2521 still ships the stale section). It therefore resolves to nothing and
	// UEngine::CreateNetDriver_Local silently falls back to DriverClassNameFallback --
	// IpNetDriver -- producing a session that hosts fine and is unjoinable across NAT, with no
	// error at create time.
	//
	// And NOT /Script/SocketSubsystemSteamIP.SteamNetDriver either: that is the legacy
	// ISteamNetworking path without NAT punch-through.
	return UCSOnlineSettings::Get().SteamNetDriverClassName;
}

// -----------------------------------------------------------------------------------
// Identity
// -----------------------------------------------------------------------------------

bool FCSOnlineBackend_Steam::BuildLoginCredentials(int32 /*LocalUserNum*/, FOnlineAccountCredentials& /*OutCreds*/) const
{
	// Steam needs no credentials, so OutCreds is left untouched.
	// FOnlineIdentitySteam::Login ignores the FOnlineAccountCredentials argument entirely and
	// only checks SteamUser()->BLoggedOn() (OnlineIdentityInterfaceSteam.cpp:33-42), and
	// GetUniquePlayerId(0) already returns a valid SteamID before any Login call (:117-125).
	// The facade still runs its login step to normalise the flow across backends and to get
	// the completion delegate; it just has nothing to fill in here.
	return false;
}

// -----------------------------------------------------------------------------------
// Session shaping
// -----------------------------------------------------------------------------------

void FCSOnlineBackend_Steam::FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const
{
	CSSteam_WarnIfLobbiesDisabled(Config);

	ApplyCommonCreateSettings(InOutSettings, Params, Config);

	// HARD REQUIREMENT, and the reason these two lines cannot live in the shared helper:
	// FOnlineSessionSteam::CreateSession fires OnCreateSessionComplete(false) and returns
	// before doing anything at all when bUsesPresence != bUseLobbiesIfAvailable
	// (OnlineSessionInterfaceSteam.cpp:229-236), and JoinSession repeats the identical check
	// against the settings carried by the search result (:861-867). EIK enforces neither, so
	// code that sets only one of the two works on EIK and fails on Steam only.
	//
	// Both are written from UsesLobbies() -- the single source of truth read by
	// FillSearchSettings as well -- so create and find cannot drift apart.
	InOutSettings.bUsesPresence          = UsesLobbies();
	InOutSettings.bUseLobbiesIfAvailable = UsesLobbies();

	// No Steam RTC voice room; this project does not consume it.
	InOutSettings.bUseLobbiesVoiceChatIfAvailable = false;

	// Visibility, mapped through BuildLobbyType (OnlineSessionAsyncLobbySteam.cpp:70-96).
	// This is the one place Steam genuinely reads bShouldAdvertise -- everywhere else it is
	// advisory. BuildLobbyType tests, in order:
	//     bIsLANMatch or !bShouldAdvertise            -> k_ELobbyTypePrivate
	//     bAllowJoinViaPresenceFriendsOnly            -> k_ELobbyTypeFriendsOnly
	//     bAllowInvites && !bAllowJoinViaPresence     -> k_ELobbyTypePrivate
	//     otherwise                                   -> k_ELobbyTypePublic
	switch (Params.Visibility)
	{
	case ECSSessionVisibility::FriendsOnly:
		InOutSettings.bShouldAdvertise                 = true;
		InOutSettings.bAllowJoinViaPresence            = true;
		InOutSettings.bAllowJoinViaPresenceFriendsOnly = true;
		break;

	case ECSSessionVisibility::InviteOnly:
		// k_ELobbyTypePrivate: the lobby is never returned by RequestLobbyList, so it is
		// invisible to FindSessions by design. Invites still work -- ApplyCommonCreateSettings
		// leaves bAllowInvites true.
		InOutSettings.bShouldAdvertise                 = false;
		InOutSettings.bAllowJoinViaPresence            = false;
		InOutSettings.bAllowJoinViaPresenceFriendsOnly = false;
		break;

	case ECSSessionVisibility::Public:
	default:
		InOutSettings.bShouldAdvertise                 = true;
		InOutSettings.bAllowJoinViaPresence            = true;
		InOutSettings.bAllowJoinViaPresenceFriendsOnly = false;
		break;
	}

	// bIsLANMatch (carried in by ApplyCommonCreateSettings) short-circuits every line above:
	// CreateSession routes to CreateLANSession and never touches the lobby API at all
	// (OnlineSessionInterfaceSteam.cpp:256-270). The presence/lobby equality check at :229
	// still runs first, though, so both flags must be set even for a LAN match.

	// Capacity note for whatever draws the session list: for a non-dedicated session Steam
	// seeds NumOpenPublicConnections = NumPublicConnections - 1
	// (OnlineSessionInterfaceSteam.cpp:248) because the host consumes a slot. MaxPlayers=8
	// therefore advertises 7 joinable slots, which is what the UI should say.
}

void FCSOnlineBackend_Steam::FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	CSSteam_WarnIfLobbiesDisabled(Config);

	InOutSearch.bIsLanQuery      = Params.bLANQuery;
	InOutSearch.MaxSearchResults = Params.MaxResults;

	// MANDATORY, and the find-side half of the UsesLobbies() invariant.
	// FOnlineSessionSteam::FindInternetSession reads SEARCH_LOBBIES and queues
	// FOnlineAsyncTaskSteamFindLobbies only when it is present AND true
	// (OnlineSessionInterfaceSteam.cpp:774-790); otherwise it queues FindServers -- the server
	// browser -- which returns nothing whatsoever for lobby hosts.
	//
	// SEARCH_LOBBIES is purely a routing key: CreateQuery explicitly skips it when building
	// the Steam filter list (OnlineSessionAsyncLobbySteam.cpp:852-856), so its bool type is
	// fine here and only here.
	if (UsesLobbies())
	{
		InOutSearch.QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	// The server-side discriminator, and on appid 480 the only one that works:
	// RequestLobbyList returns every Spacewar lobby on Steam. A String param survives
	// CreateQuery and becomes AddRequestLobbyListStringFilter on "CSGAMEID_s"
	// (SteamSessionKeys.h:56-57 appends the "_s" type suffix), filtering before results ever
	// reach ShouldAcceptSearchResult. An empty value would be dropped with "Empty search
	// parameter", so do not send one.
	if (Params.bFilterByGameIdTag && !Config.GameIdTag.IsEmpty())
	{
		InOutSearch.QuerySettings.Set(Config.GameIdTagKey, Config.GameIdTag, EOnlineComparisonOp::Equals);
	}

	// DO NOT add "PRESENCESEARCH" here, even though FCSOnlineBackend_EIK does. It is a bool,
	// and while SessionKeyToSteamKey happily names it "PRESENCESEARCH_b" and returns true
	// (SteamSessionKeys.h:62-63), CreateQuery's filter switch has cases for Int32, Float and
	// String only -- Bool falls through to "Unable to set search parameter" and is dropped
	// (OnlineSessionAsyncLobbySteam.cpp:880-913). The asymmetry with the EIK adapter is
	// correct; do not "fix" it.
	//
	// DO NOT add SETTING_MAPNAME, SEARCH_DEDICATED_ONLY, SEARCH_EMPTY_SERVERS_ONLY or
	// SEARCH_SECURE_SERVERS_ONLY either. They are game-server-browser keys and CreateQuery
	// skips them outright for lobby searches (OnlineSessionAsyncLobbySteam.cpp:852-856), so
	// they narrow nothing and only make the query look like it filters more than it does.
}

bool FCSOnlineBackend_Steam::ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	// A result without a valid OwningUserId or SessionInfo cannot be joined or displayed.
	if (!Result.IsValid())
	{
		return false;
	}

	// Belt and braces on top of the server-side filters, both of which have gaps:
	// Steam discards BuildUniqueId mismatches while parsing lobby data
	// (OnlineSessionAsyncLobbySteam.cpp:395-414), but only for lobbies that actually
	// advertised the key, and the CSGAMEID_s filter is skipped entirely when
	// bFilterByGameIdTag is off. Re-checking here costs nothing and keeps the accept rule
	// identical across backends.
	if (Params.bFilterByBuildId && !MatchesBuildId(Result))
	{
		return false;
	}

	if (Params.bFilterByGameIdTag && !MatchesGameIdTag(Result, Config))
	{
		return false;
	}

	return true;
}

#endif // CS_WITH_STEAM
