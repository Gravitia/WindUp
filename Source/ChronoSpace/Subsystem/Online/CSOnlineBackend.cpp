// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/Online/CSOnlineBackend.h"

#include "Modules/ModuleManager.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemTypes.h"
#include "Settings/CSOnlineSettings.h"
#include "ChronoSpace.h"

/**
 * Non-virtual conveniences and the shared static helpers of ICSOnlineBackend.
 *
 * ZERO #if GUARDS AND ZERO BACKEND NAME STRINGS IN THIS FILE, DELIBERATELY.
 * Everything here is expressed in terms of the virtuals (GetSubsystemName(), GetBackendId())
 * and UCSOnlineSettings. The per-backend guards live in the concrete adapters and in
 * FCSOnlineBackendRegistry, which is the only file that names both of them.
 */

// ---------------------------------------------------------------------------
// Non-virtual conveniences
// ---------------------------------------------------------------------------

IOnlineSubsystem* ICSOnlineBackend::GetSubsystem() const
{
	const FName SubsystemName = GetSubsystemName();

	// NAME_None is not "no preference" here, it is a bug. IOnlineSubsystem::Get() defaults its
	// argument to NAME_None (OnlineSubsystem.h:167-172) and that resolves DefaultPlatformService
	// -- the exact global this adapter layer exists to route around. Never call
	// IOnlineSubsystem::Get() with no argument anywhere in this layer: the whole design depends
	// on addressing backends explicitly so DefaultPlatformService is irrelevant to us. An adapter
	// whose name came back empty (e.g. someone blanked EIKSubsystemName in the ini) must fail
	// loudly rather than quietly start driving whatever backend the ini happens to name.
	if (SubsystemName.IsNone())
	{
		UE_LOG(LogCS, Error,
			TEXT("[Online] Backend %s returned an empty subsystem name; refusing to fall through to DefaultPlatformService. Check UCSOnlineSettings."),
			UCSOnlineSettings::LexToString(GetBackendId()));
		return nullptr;
	}

	// IOnlineSubsystem::Get() resolves through FModuleManager::GetModuleChecked, which asserts
	// when the OnlineSubsystem module is not loaded. IsAvailable() is contractually "cheap and
	// safe to call at any time" and FCSOnlineBackendRegistry is a lazy function-local static, so
	// the first touch can in principle be very early. Check the way IOnlineSubsystem's own
	// DoesInstanceExist/IsLoaded helpers do (OnlineSubsystem.h:241-264) instead of gambling.
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("OnlineSubsystem")))
	{
		return nullptr;
	}

	// Name-based, runtime resolution: the module is found by string concatenation
	// ("OnlineSubsystem" + Name) and loaded through FModuleManager, so the game module carries
	// no compile-time or link-time dependency on any backend plugin.
	return IOnlineSubsystem::Get(SubsystemName);
}

IOnlineSessionPtr ICSOnlineBackend::GetSessionInterface() const
{
	// Returns an empty TSharedPtr rather than dereferencing. This is the specific fix for
	// CSEIKSubsystem.cpp:122-124, :147-149, :200-202, :251-253, which each do
	// IOnlineSubsystem::Get(...)->GetSessionInterface() with no null check and reliably crash
	// when the backend is not running -- masked today only because EIK is always present.
	if (IOnlineSubsystem* Subsystem = GetSubsystem())
	{
		return Subsystem->GetSessionInterface();
	}

	return IOnlineSessionPtr();
}

IOnlineIdentityPtr ICSOnlineBackend::GetIdentityInterface() const
{
	// Same contract as GetSessionInterface(): never dereference a null subsystem.
	if (IOnlineSubsystem* Subsystem = GetSubsystem())
	{
		return Subsystem->GetIdentityInterface();
	}

	return IOnlineIdentityPtr();
}

bool ICSOnlineBackend::IsLoggedIn(int32 LocalUserNum) const
{
	const IOnlineIdentityPtr Identity = GetIdentityInterface();

	// UsingLocalProfile is NOT logged in for our purposes -- it means a local profile with no
	// platform authentication, which cannot create or join an online session.
	return Identity.IsValid() && Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

// ---------------------------------------------------------------------------
// Shared static helpers
// ---------------------------------------------------------------------------

void ICSOnlineBackend::ApplyCommonCreateSettings(FOnlineSessionSettings& InOut,
                                                 const FCSHostSessionParams& Params,
                                                 const UCSOnlineSettings& Config)
{
	// Clamped to >= 1 rather than trusted: FindEOSSession unconditionally injects a
	// NumPublicConnections >= 1 search attribute (OnlineSessionEOS.cpp:2414-2415), so a session
	// created with zero public connections is invisible to every search on the EIK Sessions path.
	// The one intentional exception is EIK's InviteOnly case, which moves the capacity to private
	// AFTER this runs and logs that the session is deliberately undiscoverable.
	InOut.NumPublicConnections = FMath::Max(1, Params.MaxPlayers);
	InOut.NumPrivateConnections = 0;

	InOut.bAllowJoinInProgress = Params.bAllowJoinInProgress;
	InOut.bIsLANMatch = Params.bIsLANMatch;
	InOut.bIsDedicated = false;
	InOut.bAllowInvites = true;

	// No anti-cheat is wired anywhere in this project (Docs/DedicatedServer_Migration.md:531).
	// Advertising it would be a lie clients could filter on.
	InOut.bAntiCheatProtected = false;
	InOut.bUsesStats = false;

	// DELIBERATELY NOT SET HERE: bShouldAdvertise, bUsesPresence, bUseLobbiesIfAvailable,
	// bUseLobbiesVoiceChatIfAvailable, bAllowJoinViaPresence, bAllowJoinViaPresenceFriendsOnly.
	// Those belong to UsesLobbies() and to each backend's visibility mapping, and the backends
	// disagree hard about them: FOnlineSessionSteam::CreateSession fires
	// OnCreateSessionComplete(false) and returns before doing anything when
	// bUsesPresence != bUseLobbiesIfAvailable (OnlineSessionInterfaceSteam.cpp:229-236, repeated
	// for JoinSession at :861), while EIK ignores that rule and derives its EOS PermissionLevel
	// from the connection counts instead. A shared default would be wrong for one of them, and
	// wrong here fails silently. Set them in FillCreateSettings, per adapter.

	// Everything below is advertised. The advertisement type must be >= ViaOnlineService or the
	// key never leaves the process: Steam skips anything lower while building its lobby
	// key/value pairs (OnlineSessionAsyncLobbySteam.cpp:130) and EIK skips it while building EOS
	// attributes (OnlineSessionEOS.cpp:1376). ViaOnlineService is the honest minimum and is
	// exactly what both backends' filters compare against; ViaOnlineServiceAndPing would buy
	// nothing, because Steam's SessionKeyToSteamKey mangles by data *type*, not by advertisement
	// mode. Using the keys off UCSOnlineSettings (rather than literals per adapter) is what stops
	// the create side and the find side from drifting apart on spelling.
	const FString EffectiveGameIdTag = Params.GameIdTagOverride.IsEmpty() ? Config.GameIdTag : Params.GameIdTagOverride;
	InOut.Set(Config.GameIdTagKey, EffectiveGameIdTag, EOnlineDataAdvertisementType::ViaOnlineService);

	// SessionDisplayName arrives already resolved: the nickname fallback documented on
	// FCSHostSessionParams belongs to UCSOnlineSessionSubsystem, because this helper is static and
	// must not reach for an identity interface. An empty value is not fatal but Steam drops it
	// with a "Empty session setting" Warning (OnlineSessionAsyncLobbySteam.cpp:136-141), so the
	// browser row would come back blank.
	InOut.Set(Config.SessionNameKey, Params.SessionDisplayName, EOnlineDataAdvertisementType::ViaOnlineService);

	// Advertised so a search result can show the map without loading it;
	// UCSOnlineSessionSubsystem::RebuildCachedResults reads this key straight back into
	// FCSSessionSearchResult::MapName. Short asset name only -- a full /Game/... path is both
	// useless in a browser row and long enough to matter against the backends' attribute limits.
	// Null TravelLevel falls back to Config.DefaultHostLevel, matching the documented behaviour of
	// FCSHostSessionParams::TravelLevel and BuildListenTravelURL, so the advertised map name can
	// never disagree with the map the host actually travels to.
	const TSoftObjectPtr<UWorld>& LevelPtr = Params.TravelLevel.IsNull() ? Config.DefaultHostLevel : Params.TravelLevel;
	const FString MapShortName = LevelPtr.IsNull() ? FString() : LevelPtr.GetAssetName();
	InOut.Set(SETTING_MAPNAME, MapShortName, EOnlineDataAdvertisementType::ViaOnlineService);
}

bool ICSOnlineBackend::MatchesGameIdTag(const FOnlineSessionSearchResult& Result,
                                        const UCSOnlineSettings& Config)
{
	// NOTE ON THE SIGNATURE: this helper receives no FCSFindSessionsParams, so it cannot honour
	// FCSFindSessionsParams::bFilterByGameIdTag itself. ShouldAcceptSearchResult() does receive
	// the params, so the opt-out is the caller's guard:
	//     return MatchesBuildId(Result) && (!Params.bFilterByGameIdTag || MatchesGameIdTag(Result, Config));
	// "Filtering is off" at this level means there is nothing configured to filter on.
	if (Config.GameIdTag.IsEmpty() || Config.GameIdTagKey.IsNone())
	{
		return true;
	}

	FString AdvertisedTag;
	if (!Result.Session.SessionSettings.Get(Config.GameIdTagKey, AdvertisedTag))
	{
		// Not one of ours. On Steam appid 480 RequestLobbyList hands back every Spacewar lobby on
		// the service, so a missing tag is the common case here, not an anomaly -- which is why
		// this returns quietly instead of warning.
		return false;
	}

	// Case-insensitive on purpose, and spelled out rather than left to FString::operator==, which
	// is already case-insensitive in Unreal and would hide the intent. The backends' own filters
	// (Steam's AddRequestLobbyListStringFilter on CSGAMEID_s, EIK's EOS attribute comparison) are
	// the real authority; this is belt-and-braces, so it must fail OPEN. A backend that ever
	// normalised attribute case would otherwise empty the session list with no diagnostic at all.
	return AdvertisedTag.Equals(Config.GameIdTag, ESearchCase::IgnoreCase);
}

bool ICSOnlineBackend::MatchesBuildId(const FOnlineSessionSearchResult& Result)
{
	// Steam already discards mismatches server-side ("Removed incompatible build",
	// OnlineSessionAsyncLobbySteam.cpp:405-415) and the EIK *Sessions* path filters by bucket id
	// (OnlineSessionEOS.cpp:2417), but the EIK *lobby* path filters by neither -- so this check
	// must exist client-side or a lobby search returns every build's sessions.
	//
	// GetBuildUniqueId() is FNetworkVersion::GetNetworkCompatibleChangelist() unless
	// [OnlineSubsystem] bUseBuildIdOverride / BuildIdOverride are set in DefaultEngine.ini
	// (OnlineSubsystem.cpp:213-255). That means an editor host and a packaged client can legally
	// disagree; when mixed-build testing is needed, set the override rather than removing this
	// filter. FCSFindSessionsParams::bFilterByBuildId is the caller's opt-out -- this helper
	// receives no params, same as MatchesGameIdTag above.
	const int32 LocalBuildId = GetBuildUniqueId();
	const int32 ResultBuildId = Result.Session.SessionSettings.BuildUniqueId;

	if (ResultBuildId == LocalBuildId)
	{
		return true;
	}

	UE_LOG(LogCS, Verbose,
		TEXT("[Online] Rejecting session from '%s': BuildUniqueId 0x%08x != local 0x%08x."),
		*Result.Session.OwningUserName, ResultBuildId, LocalBuildId);
	return false;
}
