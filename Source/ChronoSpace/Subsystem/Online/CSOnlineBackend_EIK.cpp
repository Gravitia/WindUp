// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/Online/CSOnlineBackend_EIK.h"

// THE GUARD WRAPS EVERYTHING BELOW, DELIBERATELY -- matching CSOnlineBackend_EIK.h.
// With CS_WITH_EIK=0 the header declares no type at all, so every definition here would be
// orphaned. The remaining includes live inside the guard for the same reason: nothing in
// this translation unit has any meaning when the adapter is compiled out.
//
// This is a POLICY guard, not a link guard. IOnlineSubsystem::Get(FName) resolves the DLL
// by string concatenation "OnlineSubsystem" + Name through FModuleManager at runtime
// (OnlineSubsystemModule.cpp:22-42, :382), so nothing here links against the EIK plugin and
// the game module needs no extra build dependency.
#if CS_WITH_EIK

#include "ChronoSpace.h"
#include "Common/CSLoginIdLibrary.h"
#include "Modules/ModuleManager.h"
#include "Online/OnlineSessionNames.h"
#include "Settings/CSOnlineSettings.h"

// --- identity of the adapter ---------------------------------------------------------

FName FCSOnlineBackend_EIK::GetSubsystemName() const
{
	// There is genuinely no macro to use here. Grepping the plugin for EIK_SUBSYSTEM /
	// EOS_SUBSYSTEM returns nothing; OnlineSubsystemModuleEIK.cpp:66 registers a bare "EIK"
	// string literal. The name is therefore surfaced through settings rather than hardcoded,
	// and stays a pure runtime lookup with no link-time dependency.
	return UCSOnlineSettings::Get().EIKSubsystemName;
}

FText FCSOnlineBackend_EIK::GetDisplayName() const
{
	return NSLOCTEXT("ChronoSpace", "Backend_EIK", "Epic Online Services (EIK)");
}

bool FCSOnlineBackend_EIK::IsAvailable() const
{
	// Deliberately does NOT check login state. UCSOnlineSessionSubsystem drives login, and
	// this must stay cheap and safe to call at any time, including before any login exists.
	// Every getter below null-checks internally, so nothing here can dereference null.
	return GetSubsystem() != nullptr
		&& GetSessionInterface().IsValid()
		&& GetIdentityInterface().IsValid();
}

FString FCSOnlineBackend_EIK::GetUnavailableReason() const
{
	const FName SubsystemName = GetSubsystemName();
	const FString SubsystemString = SubsystemName.ToString();

	// Checked FIRST, and the order matters. IOnlineSubsystem::IsEnabled covers both
	// [OnlineSubsystemEIK] bEnabled=false and the -no<Name> command-line switch
	// (OnlineSubsystem.cpp:412 for the command line, :479-499 for the config read). When it
	// is false, Get() returns null for a reason that has nothing to do with a missing
	// module, and reporting "module not loaded" would send the reader hunting in the
	// wrong place.
	if (!IOnlineSubsystem::IsEnabled(SubsystemName))
	{
		return FString::Printf(
			TEXT("[OnlineSubsystem%s] bEnabled=false in DefaultEngine.ini, or -no%s was passed on the command line."),
			*SubsystemString, *SubsystemString);
	}

	if (GetSubsystem() == nullptr)
	{
		// Pure string concatenation, exactly as GetOnlineModuleName does
		// (OnlineSubsystemModule.cpp:22-42). This is also why this adapter compiles without
		// any build reference to the EIK plugin.
		const FString ModuleName = FString(TEXT("OnlineSubsystem")) + SubsystemString;

		if (!FModuleManager::Get().IsModuleLoaded(FName(*ModuleName)))
		{
			return FString::Printf(
				TEXT("Module %s is not loaded. Check that the EOSIntegrationKit plugin is enabled in ChronoSpace.uproject and that its OnlineSubsystemEIK module built."),
				*ModuleName);
		}

		// The module is present but the factory refused to produce a subsystem. This is the
		// silent failure the whole GetUnavailableReason() hook exists for: LoadDefaultSubsystem
		// falls back to NULL and logs the real cause only at LogOnline Verbose
		// (OnlineSubsystemModule.cpp:216-221), so without this line the symptom is simply
		// "online does nothing".
		return FString::Printf(
			TEXT("Module %s is loaded but IOnlineSubsystem::Get(\"%s\") returned null; the EIK factory refused to create the subsystem. Check the product/sandbox/deployment ids under [/Script/EOSIntegrationKit.EIKSettings], then re-run with -LogCmds=\"LogOnline Verbose\" to see the real cause (OnlineSubsystemModule.cpp:216-221 logs it at Verbose only)."),
			*ModuleName, *SubsystemString);
	}

	const bool bHasSession = GetSessionInterface().IsValid();
	const bool bHasIdentity = GetIdentityInterface().IsValid();
	if (!bHasSession || !bHasIdentity)
	{
		return FString::Printf(
			TEXT("EIK subsystem exists but the %s%s%s interface is missing."),
			bHasSession ? TEXT("") : TEXT("session"),
			(!bHasSession && !bHasIdentity) ? TEXT(" and ") : TEXT(""),
			bHasIdentity ? TEXT("") : TEXT("identity"));
	}

	return FString();
}

// --- transport -----------------------------------------------------------------------

FString FCSOnlineBackend_EIK::GetNetDriverClassName() const
{
	// The connect string EIK hands back is only usable while this driver is live with
	// bIsUsingP2PSockets=true. With that flag false, CreateEOSSession silently advertises
	// 127.0.0.1 (OnlineSessionEOS.cpp:1512, the "This is basically ignored" branch), so
	// CreateSession still reports success and every join fails later at travel time with no
	// error anywhere near the cause.
	return UCSOnlineSettings::Get().EIKNetDriverClassName;
}

// --- THE invariant -------------------------------------------------------------------

bool FCSOnlineBackend_EIK::UsesLobbies() const
{
	// Single source of truth, read by BOTH FillCreateSettings and FillSearchSettings.
	// Never branch on anything else for this decision: EIK routes JoinSession on the flag
	// carried by the search result (OnlineSessionEOS.cpp:2661), so a create/find
	// disagreement takes the wrong join branch and fails with no useful error.
	return UCSOnlineSettings::Get().bEIKUseLobbies;
}

// --- identity ------------------------------------------------------------------------

const TCHAR* FCSOnlineBackend_EIK::GetDeviceIdCredentialType()
{
	// THIS IS THE SINGLE MOST FRAGILE STRING IN THE SYSTEM.
	//
	// It is compared with a plain FString == at UserManagerEOS.cpp:1077
	//   if (UserManager->LocalUserNumToLastLoginCredentials[0].Get().Type == "noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN")
	// to decide whether to auto-CreateDeviceID on first launch. ANY deviation -- including
	// the fully-qualified EEIK_EExternalCredentialType::EIK_ECT_... form that EIK's own
	// Blueprint node produces at EIK_Login_AsyncFunction.cpp:72 -- makes first launch on a
	// fresh machine return EOS_NotFound and fail instead of provisioning the device id.
	//
	// Do NOT reconstruct it from an enum, and do NOT "tidy" the noeas_+_ prefix.
	return TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN");
}

bool FCSOnlineBackend_EIK::BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const
{
	// EIK hardcodes local user 0 all the way down the device-id path: every entry point
	// declares `int32 LocalUserNum = 0;` (UserManagerEOS.cpp:454, :505, :600, and :663
	// inside LoginViaConnectInterface), the completion callback passes the literal 0
	// (:1073), and the first-run fallback indexes LocalUserNumToLastLoginCredentials[0]
	// (:1077). Any other index silently will not work, so refuse loudly rather than issue
	// credentials that cannot succeed.
	if (LocalUserNum != 0)
	{
		UE_LOG(LogCS, Warning,
			TEXT("[Online][EIK] BuildLoginCredentials refused LocalUserNum=%d. EIK's device-id login path is hardcoded to local user 0 (UserManagerEOS.cpp:663, :1073, :1077)."),
			LocalUserNum);
		return false;
	}

	OutCreds.Type = GetDeviceIdCredentialType();

	// UCSLoginIdLibrary, NOT FGuid::NewGuid(). CSEIKSubsystem.cpp:58 mints a fresh device id
	// every launch, so the EOS account was never stable across sessions. The library
	// persists to GGameUserSettingsIni, honours -LoginId= and clamps to 32 chars -- and it
	// is what the live Blueprints already call, so C++ and BP finally agree on one identity.
	OutCreds.Id = UCSLoginIdLibrary::GetPersistentDeviceLoginId();
	OutCreds.Token = FString();

	if (OutCreds.Id.IsEmpty())
	{
		UE_LOG(LogCS, Warning,
			TEXT("[Online][EIK] GetPersistentDeviceLoginId() returned an empty id. EOS cannot key a stable device account on an empty id and the login will fail."));
	}

	return true;
}

// --- session shaping -----------------------------------------------------------------

void FCSOnlineBackend_EIK::FillCreateSettings(FOnlineSessionSettings& InOutSettings, const FCSHostSessionParams& Params, const UCSOnlineSettings& Config) const
{
	ApplyCommonCreateSettings(InOutSettings, Params, Config);

	// One read of THE invariant, shared with FillSearchSettings below.
	InOutSettings.bUseLobbiesIfAvailable = UsesLobbies();
	InOutSettings.bUseLobbiesVoiceChatIfAvailable = false;

	// EIK advertises the raw "PRESENCESEARCH" attribute on every session it creates
	// (OnlineSessionEOS.cpp:1318). FillSearchSettings queries that same literal, which is
	// the pairing that makes presence discovery work at all on this backend.
	InOutSettings.bUsesPresence = true;
	InOutSettings.bAllowJoinViaPresence = (Params.Visibility != ECSSessionVisibility::InviteOnly);
	InOutSettings.bAllowJoinViaPresenceFriendsOnly = (Params.Visibility == ECSSessionVisibility::FriendsOnly);

	// Set truthfully even though EIK reads it only on the LAN path, where it gates the LAN
	// beacon host (OnlineSessionEOS.cpp:1210). Online visibility comes from PermissionLevel,
	// not from this flag.
	InOutSettings.bShouldAdvertise = true;

	// NOTE for anyone auditing visibility on EOS: EIK derives the permission level purely
	// from NumPublicConnections and bAllowJoinViaPresence (OnlineSessionEOS.cpp:1231-1242
	// for sessions, :3907-3918 for lobbies). It never reads
	// bAllowJoinViaPresenceFriendsOnly, so FriendsOnly is advertised exactly like Public on
	// EOS and the friends restriction is a client-side/UI concern here, not a server-side
	// one. Do not assume EOS is enforcing it.

	if (Params.Visibility == ECSSessionVisibility::InviteOnly)
	{
		// Explicit because EIK derives the permission level from the connection counts and
		// nothing else: NumPublicConnections > 0 means PublicAdvertised
		// (OnlineSessionEOS.cpp:1231, and :3907 on the lobby path). Moving the whole
		// capacity to private is the only way to reach InviteOnly. Lobby capacity is
		// preserved because GetLobbyMaxMembersFromSessionSettings sums private + public
		// (:3925).
		InOutSettings.NumPublicConnections = 0;
		InOutSettings.NumPrivateConnections = FMath::Max(1, Params.MaxPlayers);

		UE_LOG(LogCS, Log,
			TEXT("[Online][EIK] Invite-only session: NumPublicConnections=0, NumPrivateConnections=%d. This session is intentionally undiscoverable through FindSessions -- FindEOSSession injects a NumPublicConnections >= 1 filter (OnlineSessionEOS.cpp:2414-2415). Joins must arrive via invite or presence."),
			InOutSettings.NumPrivateConnections);
	}
}

void FCSOnlineBackend_EIK::FillSearchSettings(FOnlineSessionSearch& InOutSearch, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	InOutSearch.bIsLanQuery = Params.bLANQuery;
	InOutSearch.MaxSearchResults = FMath::Max(1, Params.MaxResults);

	if (UsesLobbies())
	{
		// SEARCH_LOBBIES is FName(TEXT("LOBBYSEARCH")), OnlineSessionNames.h:151.
		// Same read of THE invariant as FillCreateSettings, so create and find cannot drift.
		InOutSearch.QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	else
	{
		// RAW LITERAL ON PURPOSE. SEARCH_PRESENCE DOES NOT EXIST IN UE 5.8 -- there are zero
		// hits for PRESENCESEARCH or SEARCH_PRESENCE in
		// Engine/Plugins/Online/OnlineBase/Source/Public/Online/OnlineSessionNames.h.
		// EIK writes the raw string itself: OnlineSessionEOS.cpp:1318 is
		// `const FString SearchPresence("PRESENCESEARCH");` under the ENGINE_MINOR_VERSION >= 5
		// branch, with the SEARCH_PRESENCE branch at :1320 dead on 5.8. Do not "fix" this
		// into a macro; there is no macro to fix it into.
		InOutSearch.QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
	}

	if (Params.bFilterByGameIdTag)
	{
		// FCSFindSessionsParams carries no per-call tag override, so the effective tag is
		// always the configured one -- which is exactly what ApplyCommonCreateSettings
		// advertises when FCSHostSessionParams::GameIdTagOverride is empty.
		const FString EffectiveGameIdTag = Config.GameIdTag;
		if (!EffectiveGameIdTag.IsEmpty())
		{
			InOutSearch.QuerySettings.Set(Config.GameIdTagKey, EffectiveGameIdTag, EOnlineComparisonOp::Equals);
		}
		else
		{
			UE_LOG(LogCS, Warning,
				TEXT("[Online][EIK] bFilterByGameIdTag is set but UCSOnlineSettings::GameIdTag is empty; skipping the '%s' filter. Sending an empty-string equality filter would silently match nothing."),
				*Config.GameIdTagKey.ToString());
		}
	}

	// DO NOT add SEARCH_MINSLOTSAVAILABLE, SEARCH_EMPTY_SERVERS_ONLY or SEARCH_KEYWORDS
	// here. EIK forwards every entry of QuerySettings.SearchParams verbatim as an EOS
	// attribute comparison (OnlineSessionEOS.cpp:2420-2422), and nothing on the create side
	// advertises those keys -- so each one added is an equality test against a
	// non-existent attribute, and the search quietly returns zero results.
}

bool FCSOnlineBackend_EIK::ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result, const FCSFindSessionsParams& Params, const UCSOnlineSettings& Config) const
{
	// The dedicated-server track is out of scope for this build; a dedicated result cannot
	// be joined by the listen-server flow, so drop it before it reaches the UI rather than
	// letting the player pick something that will fail at travel.
	if (Result.Session.SessionSettings.bIsDedicated)
	{
		return false;
	}

	// MANDATORY on the lobby path. FindEOSSession filters the *Sessions* path by bucket id
	// (OnlineSessionEOS.cpp:2417) but StartLobbySearch applies no bucket filter at all, so
	// on the lobby path this client-side check is the only thing keeping a mismatched build
	// out of the list -- and a build mismatch otherwise only surfaces as a failed join.
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

#else // CS_WITH_EIK

// With the adapter compiled out this translation unit is empty, which is harmless for a DLL
// module but makes MSVC emit LNK4221 ("no public symbols found"). One anchor symbol costs
// nothing and keeps the build log clean.
namespace
{
	[[maybe_unused]] constexpr int CSOnlineBackendEIK_TUAnchor = 0;
}

#endif // CS_WITH_EIK
