// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/World.h"
#include "Subsystem/Online/CSOnlineTypes.h"
#include "CSOnlineSettings.generated.h"

/**
 * Developer settings for the whole online layer, surfaced under
 * Project Settings > Game > ChronoSpace Online and persisted to DefaultEngine.ini.
 *
 * Config = Engine (not Game, unlike UCSStageDataSettings / UCSAudioRoutingSettings) on
 * purpose: the backend choice has to live in the same ini as
 * [OnlineSubsystem] DefaultPlatformService and be readable at engine-startup time.
 *
 * NOTE: PreferredBackend -- the runtime override written by the in-game dev menu -- is
 * deliberately NOT a UPROPERTY here. It lives in GGameUserSettingsIni under the section
 * "/Script/ChronoSpace.CSOnlineSettings" and is read/written through GConfig directly by
 * ResolveStartupBackend() / WritePreferredBackend(). Declaring it as a Config UPROPERTY
 * would bind it to DefaultEngine.ini instead and silently break the override.
 */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "ChronoSpace Online"))
class CHRONOSPACE_API UCSOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCSOnlineSettings();

	// --- Backend ------------------------------------------------------------------

	/** Backend used when nothing higher-priority overrides it. See ResolveStartupBackend(). */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	ECSOnlineBackend DefaultBackend = ECSOnlineBackend::EIK;

	/**
	 * Walked in order when the requested backend turns out to be unavailable.
	 * Seeded by the constructor to { EIK, Steam, Null } only when empty, so a
	 * config-supplied array is never appended to.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	TArray<ECSOnlineBackend> BackendFallbackOrder;

	/**
	 * Allows UCSOnlineSessionSubsystem::SetActiveBackend to swap backends in-process.
	 * This only redirects OUR facade. Any AdvancedSessions / OnlineSubsystemUtils
	 * Blueprint node resolves through Online::GetSubsystem(World) -> DefaultPlatformService
	 * and keeps talking to the old backend. Use RequestBackendSwitchWithRestart() when the
	 * legacy Blueprint graphs must follow too.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	bool bAllowRuntimeBackendSwitch = true;

	/**
	 * Leave false. Turning this true resurrects the behaviour commented out at
	 * CSEIKSubsystem.cpp:18 and starts a network operation at GameInstance construction.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	bool bAutoLoginOnInitialize = false;

	/**
	 * EIK registers its subsystem under a bare string literal "EIK"
	 * (OnlineSubsystemModuleEIK.cpp:66). There is no EIK_SUBSYSTEM / EOS_SUBSYSTEM macro
	 * constant in the plugin, so the name is surfaced here rather than hardcoded in the
	 * adapter. IOnlineSubsystem::Get() resolves the module by pure string concatenation,
	 * so this stays a runtime lookup with no link-time dependency.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	FName EIKSubsystemName = FName(TEXT("EIK"));

	/** Defaults to STEAM_SUBSYSTEM. Exposed only for inspection / override. */
	UPROPERTY(EditAnywhere, Config, Category = "Backend")
	FName SteamSubsystemName = FName(TEXT("STEAM"));

	// --- Session ------------------------------------------------------------------

	/** Replaces the hardcoded /Game/02_Map/L_StageSize at CSEIKSubsystem.cpp:115 and :164. */
	UPROPERTY(EditAnywhere, Config, Category = "Session")
	TSoftObjectPtr<UWorld> DefaultHostLevel;

	/** Replaces the hardcoded 2 at CSEIKSubsystem.cpp:110. */
	UPROPERTY(EditAnywhere, Config, Category = "Session", meta = (ClampMin = "1", ClampMax = "64"))
	int32 DefaultMaxPlayers = 8;

	/** Value advertised under GameIdTagKey and matched on both create and find. */
	UPROPERTY(EditAnywhere, Config, Category = "Session")
	FString GameIdTag = TEXT("ChronoSpace");

	/** Session-setting key carrying GameIdTag. On Steam this becomes the CSGAMEID_s lobby filter. */
	UPROPERTY(EditAnywhere, Config, Category = "Session")
	FName GameIdTagKey = FName(TEXT("CSGAMEID"));

	/** Session-setting key carrying FCSHostSessionParams::SessionDisplayName. */
	UPROPERTY(EditAnywhere, Config, Category = "Session")
	FName SessionNameKey = FName(TEXT("CSNAME"));

	/** Watchdog for every async op; on expiry the facade fails the op with ECSOnlineOpResult::Timeout. */
	UPROPERTY(EditAnywhere, Config, Category = "Session", meta = (ClampMin = "5.0"))
	float OperationTimeoutSeconds = 30.0f;

	// --- Session | EIK ------------------------------------------------------------

	/**
	 * false keeps today's behaviour (EOS Sessions): the project currently creates without
	 * bUseLobbiesIfAvailable and searches without SEARCH_LOBBIES, which is self-consistent.
	 * true switches to EOS Lobbies, which buys host migration and the RTC voice room but
	 * changes join routing.
	 *
	 * Whatever this is, it drives BOTH create and find through
	 * FCSOnlineBackend_EIK::UsesLobbies(). Create and find MUST agree or joins fail
	 * silently: EIK routes JoinSession on the flag carried by the search result
	 * (OnlineSessionEOS.cpp:2661).
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Session|EIK")
	bool bEIKUseLobbies = false;

	// --- Session | Steam ----------------------------------------------------------

	/**
	 * Read-only by design. FCSOnlineBackend_Steam::UsesLobbies() returns true
	 * unconditionally and logs an Error if this is ever false, rather than honouring it:
	 * the non-lobby Steam path is the server browser, which needs the Steam game-server
	 * API (bInitServerOnClient=true or a dedicated server) that this project does not run.
	 *
	 * EditCondition="false" greys it out; EditConditionHides is deliberately ABSENT rather
	 * than set to false, because FPropertyNode only tests HasMetaData("EditConditionHides")
	 * (PropertyNode.cpp:1709-1710) -- writing "EditConditionHides=false" would still hide
	 * the row, which is the opposite of the intent (show it, but read-only).
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Session|Steam", meta = (EditCondition = "false", ToolTip = "Always true. The non-lobby Steam path is the server browser and requires the Steam game-server API, which this project does not use."))
	bool bSteamUseLobbies = true;

	// --- NetDriver ----------------------------------------------------------------

	/**
	 * Lets FCSNetDriverSwitcher retarget the GameNetDriver definition in place before
	 * travel. NetDriverDefinitions allows exactly one DriverClassName per
	 * DefName="GameNetDriver", so the transport cannot be chosen per-call from ini.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "NetDriver")
	bool bManageNetDriverDefinitions = true;

	/**
	 * Off by default and it should stay off. In the editor GEngine is UEditorEngine, which
	 * does not read [/Script/Engine.GameEngine], so PIE's GameNetDriver is the plain
	 * IpNetDriver from [/Script/Engine.Engine] -- which is what PIE wants, and Steam OSS is
	 * force-disabled in-editor anyway (OnlineSubsystemSteam.cpp:479-484).
	 */
	UPROPERTY(EditAnywhere, Config, Category = "NetDriver")
	bool bManageNetDriverInPIE = false;

	UPROPERTY(EditAnywhere, Config, Category = "NetDriver")
	FString EIKNetDriverClassName = TEXT("/Script/OnlineSubsystemEIK.NetDriverEIK");

	/**
	 * NOT /Script/OnlineSubsystemSteam.SteamNetDriver -- that class no longer exists in
	 * UE 5.8 and there is no CoreRedirect, so it resolves to nothing and silently falls
	 * back to IpNetDriver. And NOT /Script/SocketSubsystemSteamIP.SteamNetDriver, which is
	 * the legacy path without NAT punch-through.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "NetDriver")
	FString SteamNetDriverClassName = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");

	/** Used as DriverClassNameFallback, and as the Null backend's driver. */
	UPROPERTY(EditAnywhere, Config, Category = "NetDriver")
	FString FallbackNetDriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");

	// --- Accessors ----------------------------------------------------------------

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** The CDO, i.e. the live edited settings object. */
	static const UCSOnlineSettings& Get();

	/**
	 * Returns the *intent*, highest priority first:
	 *   1. -CSOnlineBackend=EIK|STEAM|NULL on the command line
	 *   2. [/Script/ChronoSpace.CSOnlineSettings] PreferredBackend in GGameUserSettingsIni
	 *   3. DefaultBackend, if not None
	 *   4. BackendFallbackOrder[0], if non-empty
	 *   5. ECSOnlineBackend::Null
	 *
	 * This deliberately does NOT consult IsAvailable(). Availability is resolved separately
	 * by UCSOnlineSessionSubsystem::ResolveBackend, so the log can say "requested Steam,
	 * Steam unavailable (client not running), fell back to EIK" instead of just reporting EIK.
	 */
	ECSOnlineBackend ResolveStartupBackend() const;

	/** Case-insensitive. Accepts the aliases EOS -> EIK and NONE / OFFLINE -> Null. */
	static ECSOnlineBackend ParseBackendName(const FString& In, bool& bOutOk);

	/** Canonical uppercase token for a backend: "EIK", "STEAM", "NULL", "NONE". */
	static const TCHAR* LexToString(ECSOnlineBackend Backend);

	/** Writes PreferredBackend into GGameUserSettingsIni and flushes it, so it survives a restart. */
	void WritePreferredBackend(ECSOnlineBackend Backend) const;
};
