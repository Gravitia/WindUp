# ChronoSpace Dual-OSS (EIK + Steam) 설계 스펙

> 자동 생성된 설계 문서. 구현의 근거 문서로 사용한다.

## 1. 아키텍처

## Verified corrections to the research (read these first)

Two research claims are wrong and I confirmed both against the 5.8 install. They change the design.

**(A) `SEARCH_PRESENCE` does not exist in UE 5.8.** `grep -c PRESENCESEARCH Engine/Plugins/Online/OnlineBase/Source/Public/Online/OnlineSessionNames.h` returns **0**, and `grep SEARCH_PRESENCE` on that header exits 1. The EIK dive was wrong. EIK advertises the *raw literal*: `OnlineSessionEOS.cpp:1318` is `const FString SearchPresence("PRESENCESEARCH");` under `#if UE >= 5.5`, with the `SEARCH_PRESENCE` branch at :1320 dead on 5.8. So the EIK adapter must write `FName(TEXT("PRESENCESEARCH"))` itself, and the Steam adapter must **not** send that key at all (a bool query setting is neither Int32 nor String/Float, so Steam's `CreateQuery` warns "Unable to set search parameter" and drops it). This alone justifies per-backend `FillSearchSettings`.

**(B) `bUseSteamNetworking` must be `false`, not `true`.** `SteamSocketsModule.cpp:22-39` reads it into `bOverrideSocketSubsystem` and passes it as `bMakeDefault` to `RegisterSocketSubsystem`, and `SocketSubsystemModule.cpp:100-103` does `DefaultSocketSubsystem = FactoryName` — i.e. `true` hijacks `ISocketSubsystem::Get()` **process-wide** away from platform IP. Meanwhile `USteamSocketsNetDriver::GetSocketSubsystem()` (`SteamSocketsNetDriver.cpp:399-401`) always resolves `ISocketSubsystem::Get(STEAM_SOCKETS_SUBSYSTEM)` **by name**, so SteamSockets loses nothing when the flag is false. With `true`, the IpNetDriver fallback, LAN beacons and EIK's passthrough path would all try to open Steam sockets. The engine comment at :26 says exactly this ("so that the IPNetDrivers can be used as needed"). For a dual-backend project the flag must be `false`.

**(C) Steam OSS is created eagerly once SteamSockets is enabled.** `FSteamSocketsModule::StartupModule` (`SteamSocketsModule.cpp:14`) calls `IOnlineSubsystem::Get(STEAM_SUBSYSTEM)` at module load and **latches `bEnabled` once with no retry**. So in a packaged/`-game` build, `SteamAPI_InitEx` runs at boot even when the player picked EIK. Graceful when Steam is absent (Get returns null → SteamSockets stays disabled → `USteamSocketsNetDriver::IsAvailable()` false → engine auto-falls back to `IpNetDriver` at `UnrealEngine.cpp:15020`), but it is not lazy and you cannot turn it on later in the process.

**(D) Real config line numbers** are the integration dive's, not the EIK dive's: `[OnlineSubsystemEIK]` at **193**, `[/Script/OnlineSubsystemEIK.NetDriverEIK]` at **198**, `[/Script/EOSIntegrationKit.EIKSettings]` at **202**, `[Core.Log]` at **248**.

## Layering

```
UMG / Blueprints  ─────────────────────────────┐
                                               │  BlueprintCallable + BlueprintAssignable
UCSEIKSubsystem (deprecated shim, 4 UFUNCTIONs)┤
                                               ▼
                     UCSOnlineSessionSubsystem  (UGameInstanceSubsystem)
                     ── shared flow, ZERO #if, ZERO backend strings ──
                              │ owns TSharedPtr<ICSOnlineBackend>
                              ▼
                     ICSOnlineBackend  (plain C++, non-UObject)
                              │
        ┌─────────────────────┼──────────────────────┐
   #if CS_WITH_EIK      #if CS_WITH_STEAM      (always compiled)
FCSOnlineBackend_EIK  FCSOnlineBackend_Steam  FCSOnlineBackend_Null
                              │
                     FCSOnlineBackendRegistry (lazy singleton, the ONLY #if site
                                               that names both adapters)
                              │
                     FCSNetDriverSwitcher (pure strings, no #if)
                     UCSOnlineSettings (UDeveloperSettings, Config=Engine)
```

**Why an adapter interface and not `#if` inside one class.** The user asked for compile-time isolation. Sprinkling `#if` inside a single subsystem gives you isolation but leaves the shared flow interleaved with backend trivia and makes the eventual Steam-only cutover a diff across every method. With adapters, the year-end cutover is: delete `CSOnlineBackend_EIK.{h,cpp}`, drop the EIK plugin from `.uproject`, and `CS_WITH_EIK` self-resolves to 0. Nothing in the façade changes. That is the whole point of the shape.

**Why `ICSOnlineBackend` is not a `UINTERFACE`.** It is consumed only from C++, needs `TSharedPtr` lifetime, and must be constructible before any UObject exists. A UObject interface would force GC rooting and buy nothing. The Blueprint surface lives entirely on the façade.

**Why the game module links nothing backend-specific.** Confirmed twice over: `IOnlineSubsystem::Get(FName)` resolves the DLL by pure string concatenation `"OnlineSubsystem" + Name` and `FModuleManager::LoadModule` (`OnlineSubsystemModule.cpp:22-42`, `:382`) — a *runtime, name-based* load with no compile-time symbol. `ChronoSpace.Build.cs:26-27` already has `OnlineSubsystem` + `OnlineSubsystemUtils`, and `CSEIKSubsystem.cpp` already proves it by calling `Get(TEXT("EIK"))` nine times with no EIK dependency. `STEAM_SUBSYSTEM` is declared in the *generic* plugin (`OnlineSubsystemNames.h:53`), not the Steam one, so even the Steam adapter compiles without the Steam plugin. **No new module dependencies at all.** `CS_WITH_*` is therefore purely a policy switch (don't compile an adapter for a backend that can't work), not a link requirement — which is why it is safe to have it default off.

**The single most important invariant: `UsesLobbies()`.** The create side and the find side must agree on lobbies-vs-sessions or joins fail silently and confusingly (EIK routes `JoinSession` on the flag carried *by the search result* — `OnlineSessionEOS.cpp:2661`; Steam hard-fails `CreateSession`/`JoinSession` when `bUsesPresence != bUseLobbiesIfAvailable` — `OnlineSessionInterfaceSteam.cpp:229-236`, `:861`). Making it one virtual read by **both** `FillCreateSettings` and `FillSearchSettings` removes the entire bug class structurally rather than by discipline.

## Runtime selection

Resolution order, highest wins, in `UCSOnlineSettings::ResolveStartupBackend()`:

1. `-CSOnlineBackend=EIK|STEAM|NULL` command line
2. `GGameUserSettingsIni` `[/Script/ChronoSpace.CSOnlineSettings] PreferredBackend` (written by the in-game dev menu, survives restart)
3. `DefaultBackend` from `DefaultEngine.ini`
4. First entry of `BackendFallbackOrder` whose adapter reports `IsAvailable()`
5. `Null`

`IsAvailable()` does the real work: module loadable, `IOnlineSubsystem::Get(Name)` non-null, session+identity interfaces valid, plus Steam's editor-process gate. If the chosen backend is unavailable at `Initialize()` the subsystem walks `BackendFallbackOrder` and logs a Warning naming both the requested and the actual backend. This is what makes "Steam client not running" a graceful degradation instead of the null-deref the current `CSEIKSubsystem` completion handlers would produce.

**Hard limit on runtime switching, and it is not a small one.** `SetActiveBackend` only redirects *our* façade. Any Blueprint node from AdvancedSessions or `OnlineSubsystemUtils` (`CreateSessionCallbackProxy`, `FindSessionsCallbackProxyAdvanced`, `JoinSessionCallbackProxy`, `AdvancedFriendsGameInstance`'s invite flow) resolves through `Online::GetSubsystem(World)` → `DefaultPlatformService` → **always EIK**. Since `BP_CSGameInstance`, `BP_SessionUI` and `WBP_EOSLobby` are built on exactly those nodes, a runtime switch to Steam leaves them talking to EIK while the façade talks to Steam. Two supported modes, both spelled out in the settings:

- **In-session switch** (`bAllowRuntimeBackendSwitch=true`, default): only code that goes through `UCSOnlineSessionSubsystem` follows. Correct for the C++/new-UI path; the legacy AdvancedSessions graphs must be migrated before this is trustworthy.
- **Restart switch** (recommended for QA builds): `RequestBackendSwitchWithRestart()` writes `DefaultPlatformService` into `GEngineIni` *and* `PreferredBackend` into `GGameUserSettingsIni`, then asks the player to relaunch. This moves the AdvancedSessions nodes too. Non-shipping shortcut for the same effect with no writes: `-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=STEAM -CSOnlineBackend=STEAM`.

`IOnlineSubsystem::ReloadDefaultSubsystem()` exists but its own header (`OnlineSubsystemModule.h:193-197`) says editor-only and crash-prone. Do not use it.

## Transport

`NetDriverDefinitions` allows exactly one `DriverClassName` per `DefName="GameNetDriver"`, so the transport cannot be chosen per-call from ini. `FCSNetDriverSwitcher` mutates the existing element **in place** immediately before `ServerTravel`/`ClientTravel`. In place is mandatory, not stylistic: `FNamedNetDriver::NetDriverDef` is a raw `FNetDriverDefinition*` into that array (`Engine.h:312`, stored at `UnrealEngine.cpp:15045`), so any `Add`/`Remove`/`Empty` can realloc and dangle every live driver. The switcher refuses to run while `WorldContext.ActiveNetDrivers.Num() > 0`.

Two free safety nets mean a wrong driver degrades instead of crashing: `CreateNetDriver_Local` falls back to `DriverClassNameFallback` when the class fails to load **or** `CDO->IsAvailable()` is false (`UnrealEngine.cpp:15020-15024`), and `USteamSocketsNetDriver::IsAvailable()` returns false with no Steam socket subsystem (`SteamSocketsNetDriver.cpp:41-50`) while `UNetDriverEIKBase` degrades to `UIpNetDriver` passthrough for non-`EOS:` URLs (`NetDriverEIKBase.cpp:109-115`).

PIE is guarded off by default (`bManageNetDriverInPIE=False`). In the editor `GEngine` is `UEditorEngine`, which does not read `[/Script/Engine.GameEngine]`, so PIE's GameNetDriver is the plain `IpNetDriver` from `[/Script/Engine.Engine]` — correct for PIE, and Steam OSS is force-disabled in-editor anyway (`OnlineSubsystemSteam.cpp:479-484`). Because `GEngine` outlives a PIE session, the switcher snapshots the original values once and `RestoreDefaults()` runs from `Deinitialize()` so mutations never leak into the next run.

## Shared flow, and the ordering it must not break

Host: `Login → FillCreateSettings → CreateSession → StartSession → SwitchNetDriver → ServerTravel(URL?listen)`.
Client: `FindSessions → (BP picks index) → SwitchNetDriver → JoinSession → check Result==Success → GetResolvedConnectString → ClientTravel`.

Constraints inherited from `Docs/DedicatedServer_Migration.md` and the slot subsystem, all load-bearing:

- **Session-start and travel stay coupled and in that order** (:708). `ServerTravel` fires from `OnStartSessionComplete`, never decoupled.
- **Never `DestroySession` before travel.** `ACSGameMode::Logout` releases the player slot only when `!GetWorld()->bIsTearingDown` (`CSGameMode.cpp:138-145`); a disconnect outside a tearing-down world makes every player lose their slot mid-travel and re-roll their character.
- **No new async step before `PostLogin`.** The slot is stamped onto `PlayerState` *before* `Super::PostLogin` because `RestartPlayer → GetDefaultPawnClassForController` needs it at that instant; the comments at `CSGameMode.cpp:52-53` and `:160-161` call the failure the "EOS reconnect race". Nothing in this design inserts work there.
- **Travel URL is built by the layer, not typed at call sites.** `?listen` never appears in a caller again.
- `GetResolvedConnectString` is the most backend-divergent line in the system (`EOS:<PUID>:<Socket>:<Chan>` vs `steam.<SteamID64>:<port>`). It is called exactly once, in one place, and its output is passed verbatim to `ClientTravel` — never parsed.

## Identity fallout you must decide on

`UCSPlayerSlotSubsystem::MakePlayerKey` keys on the UniqueNetId **string** (`net:<id>|<lpid>`, `CSPlayerSlotSubsystem.cpp:75`). EOS ProductUserIds and SteamID64s stringify completely differently, so `SlotByPlayerKey` is garbage across a backend switch, and the `remote:<controllerId>` fallback (`:91-92`) cannot tell two remote clients apart — if the NetId path ever returns empty on Steam, every remote player collapses into one slot. The façade broadcasts `OnBackendChanged`; hook it to `UCSPlayerSlotSubsystem::ResetAllSlots()`. Namespacing the key with the backend id is the better long-term fix but touches a class this work should not otherwise disturb — see openQuestions.

## 2. Build.cs

## `Source/ChronoSpace/ChronoSpace.Build.cs`

Two changes: add the `using` lines and the detection helper, and emit three paired defines. **No new module dependencies** — `OnlineSubsystem` + `OnlineSubsystemUtils` (lines 26-27) already cover everything, and that is provably sufficient because `IOnlineSubsystem::Get(FName)` resolves the DLL by string concatenation `"OnlineSubsystem" + Name` and loads it through `FModuleManager` at runtime (`OnlineSubsystemModule.cpp:22-42`, `:382`), with no compile-time symbol reference anywhere in the path.

```csharp
// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class ChronoSpace : ModuleRules
{
	public ChronoSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] { "ChronoSpace" });

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NavigationSystem",
			"UMG",
			"DeveloperSettings",
			"GameplayTasks",
			"Slate",
			"SlateCore",
			"OnlineSubsystem",        // generic IOnlineSubsystem / IOnlineSession / IOnlineIdentity
			"OnlineSubsystemUtils",   // FBlueprintSessionResult, BP proxies
			"Niagara",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags"
		});

		// ------------------------------------------------------------------
		// Online backend selection (see Source/ChronoSpace/Subsystem/Online).
		//
		// These defines are POLICY, not linkage. Every backend is reached
		// through IOnlineSubsystem::Get(FName), which resolves the module by
		// name at runtime (OnlineSubsystemModule.cpp:22-42), so no platform
		// OSS module is ever linked here. The defines exist only so an
		// adapter for a backend that cannot work in this configuration is
		// not compiled at all.
		//
		// PublicDefinitions (not Private) because CS_WITH_* is tested in
		// headers under Subsystem/Online that other code includes; private
		// defines do not propagate to dependents.
		//
		// Always emit BOTH 1 and 0 so `#if CS_WITH_X` is always well-defined
		// and -Wundef / MSVC C4668 never fire. Same convention the engine
		// uses and explains at AIModule.Build.cs:41-52.
		// ------------------------------------------------------------------
		bool bWithEIK          = IsProjectPluginEnabled(Target, "EOSIntegrationKit");
		bool bWithSteam        = IsProjectPluginEnabled(Target, "OnlineSubsystemSteam");
		bool bWithSteamSockets = IsProjectPluginEnabled(Target, "SteamSockets");

		PublicDefinitions.Add("CS_WITH_EIK="          + (bWithEIK          ? "1" : "0"));
		PublicDefinitions.Add("CS_WITH_STEAM="        + (bWithSteam        ? "1" : "0"));
		PublicDefinitions.Add("CS_WITH_STEAMSOCKETS=" + (bWithSteamSockets ? "1" : "0"));
	}

	/// <summary>
	/// True if PluginName is enabled in the .uproject for this platform,
	/// configuration and target type.
	///
	/// Target.IsPluginEnabledForTarget(string) DOES NOT EXIST on
	/// ReadOnlyTargetRules in UE 5.8 - do not reach for it. And
	/// Target.EnablePlugins / DisablePlugins come only from the
	/// -EnablePlugin= / -DisablePlugin= command line (TargetRules.cs:865/872);
	/// they are EMPTY for plugins enabled in the .uproject, so checking them
	/// silently evaluates false and the backend is silently never compiled.
	///
	/// Plugins.GetPlugin(name) also exists but only consults PluginInfoCache
	/// and can return null depending on when module rules are constructed
	/// (Plugins.cs:265). Reading the .uproject directly has no such ordering
	/// hazard, and mirrors the allow/deny logic UBT itself applies in
	/// Plugins.IsPluginEnabledForTarget (Plugins.cs:689).
	/// </summary>
	private static bool IsProjectPluginEnabled(ReadOnlyTargetRules Target, string PluginName)
	{
		if (Target.ProjectFile == null)
		{
			return false;
		}

		try
		{
			ProjectDescriptor Project = ProjectDescriptor.FromFile(Target.ProjectFile);
			if (Project.Plugins == null)
			{
				return false;
			}

			PluginReferenceDescriptor Ref = Project.Plugins.FirstOrDefault(
				p => String.Equals(p.Name, PluginName, StringComparison.OrdinalIgnoreCase));

			// Not listed. Note this also returns false for a plugin that is
			// EnabledByDefault and omitted - fine here, because both
			// OnlineSubsystemSteam and SteamSockets ship
			// "EnabledByDefault": false and must be listed explicitly.
			if (Ref == null || !Ref.bEnabled)
			{
				return false;
			}

			// Conservative: an optional plugin may not be present at build
			// time, so do not compile an adapter that assumes it. Matches
			// Plugins.IsPluginEnabledForTarget, which skips bOptional refs.
			if (Ref.bOptional)
			{
				return false;
			}

			return Ref.IsEnabledForPlatform(Target.Platform)
				&& Ref.IsEnabledForTargetConfiguration(Target.Configuration)
				&& Ref.IsEnabledForTarget(Target.Type);
		}
		catch (Exception)
		{
			// Malformed / unreadable .uproject: fail closed rather than
			// compiling an adapter whose plugin may not be staged.
			return false;
		}
	}
}
```

Delete the now-stale trailing comment at line 45 ("To include OnlineSubsystemSteam, add it to the plugins section in your uproject file...") — it was correct guidance, and the `.uproject` change below is exactly what it was asking for.

### Macro naming, and why

**`CS_WITH_EIK`, not `CS_WITH_EOS`.** Each macro is named after the `.uproject` plugin entry it detects (`EOSIntegrationKit`→EIK, `OnlineSubsystemSteam`→STEAM, `SteamSockets`→STEAMSOCKETS), so the define, the plugin, and the adapter file all carry one name and a reader can trace `CS_WITH_EIK` to a single line of JSON. `CS_WITH_EOS` would be actively misleading here: the engine ships a *different* `OnlineSubsystemEOS` plugin that EIK hard-conflicts with (EIK pops a modal at editor startup if `OnlineSubsystemEOS`, `EOSShared`, `EOSVoiceChat` or `SocketSubsystemEOS` are enabled — `EOSIntegrationKit.cpp:14-40`), and EIK itself already defines `WITH_EOS_SDK`. A `CS_WITH_EOS` sitting next to `WITH_EOS_SDK` and meaning "the third-party EIK plugin, not the Epic EOS plugin" is a trap.

If the team prefers the `CS_WITH_EOS` spelling anyway it is a pure rename of one string in three places — but do not emit both names for one condition; two spellings of one flag drift the moment someone guards a file with the one that was not updated.

**`CS_WITH_STEAMSOCKETS` is separate from `CS_WITH_STEAM`** because they are genuinely independent plugins with independent failure modes: Steam OSS gives you lobbies and identity, SteamSockets gives you the NAT-punching transport, and enabling the first without the second is a valid (if useless for P2P) configuration that should not silently advertise a net driver class that will not resolve.

### Also change `Source/ChronoSpace.Target.cs` — but only when a real appid exists

`SteamDevAppId` is read only in non-Shipping (`SteamSharedModule.cpp:65-129`, `OnlineSubsystemSteam.cpp:103-123`). Shipping takes the appid from `UE_PROJECT_STEAMSHIPPINGID`, which `#define`s to **0** unless the project supplies it, and there is no UBT/ini plumbing for it. Same for `UE_PROJECT_STEAMGAMEDIR`, which defaults to `"unrealtest"` and is the mandatory `gamedir` server-browser filter.

```csharp
// Add to ChronoSpaceTarget's constructor once the real appid is issued.
// Leave commented out while on 480 - a Shipping build against Spacewar is
// not something you want to produce by accident.
// GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=<your_appid>");
// GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDIR=\"chronospace\"");
```

## 3. .uproject

## `ChronoSpace.uproject` — add three entries to the `Plugins` array

Insert after the existing `EOSIntegrationKit` entry, leaving every other entry exactly as it is:

```json
		{
			"Name": "EOSIntegrationKit",
			"Enabled": true,
			"MarketplaceURL": "com.epicgames.launcher://ue/marketplace/product/dba0aa05bdd144e69405653a387ad63e"
		},
		{
			"Name": "OnlineSubsystemSteam",
			"Enabled": true
		},
		{
			"Name": "SteamShared",
			"Enabled": true
		},
		{
			"Name": "SteamSockets",
			"Enabled": true
		},
		{
			"Name": "AdvancedSteamSessions",
			"Enabled": false
		},
```

### Why each one, and what must NOT be added

**`OnlineSubsystemSteam`** — mandatory. Its `.uplugin` line 11 is `"EnabledByDefault": false`, so it must be listed explicitly; without the entry, `IsProjectPluginEnabled` returns false, `CS_WITH_STEAM=0`, and the adapter is not compiled. Listing the *module* in `PrivateDependencyModuleNames` instead is the trap to avoid: UBT does not hard-error (the rules assembly is built from all engine plugins on disk, `RulesCompiler.cs:96`), it emits only a validation warning (`UEBuildTarget.cs:1666`) and then fails at **runtime**, because a non-enabled plugin's `Config/` is never layered into the hierarchy, its `RuntimeDependencies` (`steam_api64.dll`) are never staged, and `FSteamSharedModule::AreSteamDllsLoaded()` consequently fails so the factory is never registered (`OnlineSubsystemModuleSteam.cpp:82-93`). Silent until packaged.

**`SteamShared`** — pulled in transitively by both Steam plugins' `.uplugin` dependency arrays, but listed explicitly so the dependency is visible in one place and so a future removal of one Steam plugin does not silently drop it. It writes `steam_appid.txt` and owns `SteamAPI_InitEx`.

**`SteamSockets`** — the P2P transport. This is the one that uses the Steam Datagram Relay: `FSteamSocketsSubsystem::Init` calls `InitRelayNetworkAccess()` and `InitAuthentication()` when `bAllowP2PPacketRelay=true` (`SteamSocketsSubsystem.cpp:86-165`), and it supplies the ping interface that fills lobby ping data.

**Do NOT add `SocketSubsystemSteamIP`.** It is the legacy `ISteamNetworking` path whose own description says "Does NOT use NAT punchthrough, use the SteamSockets plugin for P2P support". Worse, enabling it alongside SteamSockets is an outright conflict: both register a socket subsystem and both key off `bUseSteamNetworking` with *opposite* defaults — `SocketSubsystemSteamIPModule.cpp:26` treats a missing key as false, `SteamSocketsModule.cpp:28` treats it as true.

**Leave `AdvancedSteamSessions` at `"Enabled": false`.** It is a Steam-specific extension of AdvancedSessions and is not part of this design; flipping it on is a separate decision that changes which Blueprint nodes exist.

**Leave `EOSIntegrationKit` at `"Enabled": true`** for the whole transition year. The cutover to Steam-only is: set it to `false` (or delete the entry), delete `CSOnlineBackend_EIK.{h,cpp}`, and `CS_WITH_EIK` resolves to 0 on the next build with no other source change.

### Optional, and I recommend against it initially

You *can* add `"TargetAllowList": ["Game", "Client", "Server"]` to the three Steam entries to keep them out of Editor builds. It has a genuinely nice property — `IsProjectPluginEnabled` would then return false for the Editor target, so `CS_WITH_STEAM=0` in the editor DLL and the Steam adapter is not even compiled there, which is honest given Steam is force-disabled in the editor process anyway (`OnlineSubsystemSteam.cpp:479-484`).

But it means the editor and game DLLs have different `CS_WITH_*` values, so a `#if`-guarded compile error only shows up in one of them, and `cs.Online.Status` would report a different compiled-backend set depending on which binary you are in. During a year-long transition where people will be switching backends and reading that output constantly, one consistent answer is worth more than the saved editor compile. Add it later if editor startup time becomes a real complaint.

### Binaries consequence

Per `.claude/rules/02-generated-output.md`, this project deliberately commits `Binaries/Win64/UnrealEditor-ChronoSpace.dll`, `UnrealEditor.modules` and `ChronoSpaceEditor.target` so the `.uproject` opens on double-click. Adding these plugins changes the game module's compiled output, so **`Binaries/Win64/UnrealEditor-ChronoSpace.dll` must be rebuilt and committed with this change** or teammates' editors will load a DLL whose `CS_WITH_*` values disagree with the `.uproject`. The Steam plugins themselves are *engine* plugins — their binaries live in the UE_5.8 install, not under `Plugins/`, so there is nothing new to commit for them.

## 4. Config/DefaultEngine.ini

## `Config/DefaultEngine.ini`

Edits are described against the real current line numbers (verified: `[/Script/Engine.GameEngine]` NetDriverDefinitions at **105-108**, `[OnlineSubsystem]` at **111-112**, commented Steam blocks at **114-119**, `[OnlineSubsystemEIK]` at **193**, `[/Script/OnlineSubsystemEIK.NetDriverEIK]` at **198**, `[/Script/EOSIntegrationKit.EIKSettings]` at **202**, `[Core.Log]` at **248**).

Note the file has **two** `[/Script/Engine.Engine]` headers (74 and 83) and a separate `[/Script/Engine.GameEngine]` at 104. It is easy to edit the wrong block — all online edits below belong to the 104-119 region and the 193+ region.

### 1. Replace lines 104-119 wholesale

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
; BOOT-TIME DEFAULT ONLY.
; FCSNetDriverSwitcher rewrites DriverClassName on this entry IN PLACE before
; every ServerTravel/ClientTravel to match the active backend. Only one class
; can occupy DefName="GameNetDriver", which is why the switch has to be a
; runtime mutation rather than two ini entries.
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemEIK.NetDriverEIK",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
;
; DEAD CONFIG - DO NOT UNCOMMENT EITHER OF THESE.
; UE 5.8 removed OnlineSubsystemSteam.SteamNetDriver / SteamNetConnection; the
; classes moved to the SocketSubsystemSteamIP plugin and there is NO CoreRedirect,
; so this string resolves to nothing and silently falls back to IpNetDriver.
; The Steam transport we use is /Script/SteamSockets.SteamSocketsNetDriver,
; applied at runtime by FCSNetDriverSwitcher.
;+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
;+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemEOS.EOSNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
; Which OSS instance the engine creates at startup, and - critically - which one
; every AdvancedSessions / OnlineSubsystemUtils Blueprint proxy resolves through
; (they call Online::GetSubsystem(World), which uses this value).
; UCSOnlineSessionSubsystem does NOT depend on it: it always passes an explicit
; FName to IOnlineSubsystem::Get(). Change this only for a restart-based switch.
DefaultPlatformService=EIK
;
; NativePlatformService is deliberately UNSET. Setting it to Steam makes EIK's
; FUserManagerEOS::GetPlatformOSS() adopt Steam identity and enables the
; "Steam login -> EOS session" external-auth path (UserManagerEOS.cpp:266-306,
; :1934-1953). That is the eventual bridge configuration, not the dual-backend
; one, and turning it on now changes login behaviour under the EIK backend.
;NativePlatformService=Steam
;
; Load both OSS modules during FOnlineSubsystemModule::StartupModule. Without
; this, first-touching a backend later logs a shutdown-ordering warning
; (OnlineSubsystemModule.cpp:63). Registering a factory does NOT create an
; instance - instances stay lazy per name (OnlineSubsystemModule.cpp:382).
+AdditionalModulesToLoad=OnlineSubsystemEIK
+AdditionalModulesToLoad=OnlineSubsystemSteam
```

### 2. Line 193-199 region — EIK stays as-is

```ini
[OnlineSubsystemEIK]
bEnabled=true

[/Script/OnlineSubsystemEIK.NetDriverEIK]
; Load-bearing. With this false, CreateEOSSession quietly advertises the host
; address as "127.0.0.1" (OnlineSessionEOS.cpp:1512) with no error at create
; time, and every join fails later at travel. Read once in FOnlineSessionEOS
; init, with a fallback to the legacy [/Script/SocketSubsystemEOS.NetDriverEOSBase]
; section - make sure no stale copy of that section exists to shadow this.
bIsUsingP2PSockets=true
```

### 3. Line 203 — the single most important change in this file

```ini
[/Script/EOSIntegrationKit.EIKSettings]
; MUST BE FALSE FOR THE DUAL-BACKEND SETUP.
; When True, FEOSIntegrationKitModule::ConfigureOnlineSubsystemEIK() rewrites
; THIS FILE as raw text at every module startup (EOSIntegrationKit.cpp:42-150):
; it appends [OnlineSubsystemEIK] bEnabled=true, appends a
; [/Script/Engine.GameEngine] NetDriverDefinitions block, and FORCE-REPLACES any
; existing DefaultPlatformService= line with EIK. Every Steam edit below, and any
; runtime or restart-based switch of DefaultPlatformService, gets silently
; reverted on the next launch. A near-duplicate also runs from
; UEIKSettings::PostEditChangeProperty (EIKSettings.cpp:493-567).
bAutomaticallySetupEIK=False
```
Leave every other key in that section (`OrganizationName`, `ProductName`, `AutoLoginType`, `+LoginFlags`, `+Artifacts`, ...) untouched. Setting `bAutomaticallySetupEIK=False` does **not** disable EIK — the settings the auto-setup would have written are already present above.

### 4. New Steam block — insert after the EIK block (after line ~245)

```ini
;==============================================================================
; Steam - lobby-based listen server, coexisting with EIK.
;==============================================================================
[OnlineSubsystemSteam]
bEnabled=true
; REQUIRED in non-Shipping. If this key is missing, GetRelaunchSettings()
; returns false and FOnlineSubsystemSteam::Init() aborts with
; "Could not set up the steam environment! Falling back to another OSS."
; - the subsystem silently does not exist rather than erroring loudly.
; 480 = Spacewar, the shared public dev appid. See risks.
; NOTE: "SteamAppId" is NOT read anywhere in UE 5.8. Only SteamDevAppId.
; Shipping ignores this entirely and uses UE_PROJECT_STEAMSHIPPINGID from Target.cs.
SteamDevAppId=480

; Keep false on 480 or the process bounces through the Steam client as Spacewar.
bRelaunchInSteam=false

; Lobbies do not need the Steam game-server API. Only set true if you move to
; the server-browser path (which we do not).
bInitServerOnClient=false

; MUST BE FALSE for dual-backend.
; SteamSocketsModule.cpp:22-39 passes this as bMakeDefault to
; RegisterSocketSubsystem, and SocketSubsystemModule.cpp:100-103 then sets
; DefaultSocketSubsystem = "SteamSockets" PROCESS-WIDE - hijacking
; ISocketSubsystem::Get() away from platform IP for IpNetDriver, LAN beacons
; and EIK's passthrough path. SteamSockets loses nothing: its net driver always
; resolves ISocketSubsystem::Get(STEAM_SOCKETS_SUBSYSTEM) by name
; (SteamSocketsNetDriver.cpp:399-401). The engine's own comment at
; SteamSocketsModule.cpp:26 says exactly this.
bUseSteamNetworking=false

; Enables the Steam Datagram Relay + ping support. SteamSockets reads it from
; THIS section only (SteamSocketsSubsystem.cpp:88), which is why it appears twice.
bAllowP2PPacketRelay=true

bVACEnabled=false
GameVersion=1.0.0.0
GameServerQueryPort=27015

[SocketSubsystemSteamIP]
; New home of these keys in 5.8; the old location still works but logs a
; deprecation warning. Harmless while SocketSubsystemSteamIP is disabled.
bAllowP2PPacketRelay=true
P2PConnectionTimeout=90

[/Script/SteamSockets.SteamSocketsNetDriver]
; BaseEngine.ini:2524-2531 already supplies this plus ConnectionTimeout,
; InitialConnectTimeout and tick rates. Restated here so the pairing is visible
; next to the driver class name FCSNetDriverSwitcher installs.
NetConnectionClassName="/Script/SteamSockets.SteamSocketsNetConnection"

;==============================================================================
; ChronoSpace online layer
;==============================================================================
[/Script/ChronoSpace.CSOnlineSettings]
DefaultBackend=EIK
+BackendFallbackOrder=EIK
+BackendFallbackOrder=Steam
+BackendFallbackOrder=Null
bAllowRuntimeBackendSwitch=True
bAutoLoginOnInitialize=False
EIKSubsystemName=EIK
SteamSubsystemName=STEAM

DefaultHostLevel=/Game/02_Map/L_StageSize.L_StageSize
DefaultMaxPlayers=8
GameIdTag=ChronoSpace
GameIdTagKey=CSGAMEID
SessionNameKey=CSNAME
OperationTimeoutSeconds=30.0

; False = EOS Sessions (today's behaviour, consistent create/find).
; True  = EOS Lobbies: gains host migration and the RTC voice room, changes
;         join routing, and loses the bucket filter. Drives BOTH create and
;         find through FCSOnlineBackend_EIK::UsesLobbies() - never set one side.
bEIKUseLobbies=False

; Must stay True. The non-lobby Steam path is the server browser and needs the
; Steam game-server API. The adapter forces lobbies regardless and logs an Error.
bSteamUseLobbies=True

bManageNetDriverDefinitions=True
bManageNetDriverInPIE=False
EIKNetDriverClassName=/Script/OnlineSubsystemEIK.NetDriverEIK
SteamNetDriverClassName=/Script/SteamSockets.SteamSocketsNetDriver
FallbackNetDriverClassName=/Script/OnlineSubsystemUtils.IpNetDriver
```

### 5. Replace the `[Core.Log]` block at 248-251

The current block has `LogOnline=VeryVerbose`, which is very loud and still misses the categories that matter here.

```ini
[Core.Log]
LogOnline=Verbose
LogOnlineSession=Verbose
LogOnlineIdentity=Verbose
LogNet=Log
LogSockets=Log
LogEIK=Verbose
LogEOSVoiceChat=Warning
LogCS=Verbose
LogTemp=Verbose
```
`LogNet` and `LogSockets` are new and are the two you actually need: they are where the net-driver class resolution and the socket-subsystem registration report. EIK routes most of its net-driver messages through `LogTemp`, which is why that stays verbose. `LogEOSVoiceChat` drops to Warning since no voice is wired.

### Two things this file already has that need a decision, not an edit

- **Line ~232 commits a live EOS `ClientSecret` and `EncryptionKey`** for the ChronoSpace artifact in plaintext. Not caused by this work, but it is in the same block you are about to edit and the repo is presumably shared. Rotate and move to an untracked overlay before this goes further.
- **`TransitionMap=` is empty (line 7)** and `bUseSeamlessTravel` is set nowhere. All travel is non-seamless, which this design depends on: `ACSGameMode::HandleSeamlessTravelPlayer` does not re-stamp the player slot and `ACSPlayerState` has no `CopyProperties` override, so enabling seamless travel today would silently reset every player to slot Player0. Do not enable it as part of this work.

## 5. Steam 초기 설정

## Steam setup, in the order you should actually do it

### Phase 0 — before touching anything

Confirm the three engine plugins exist in your install (verified present on this machine): `Engine/Plugins/Online/OnlineSubsystemSteam`, `Engine/Plugins/Runtime/Steam/SteamShared`, `Engine/Plugins/Runtime/Steam/SteamSockets`. The Steamworks SDK bundled with 5.8 is **v1.64** (`Steamworks.build.cs:13`, headers under `Engine/Source/ThirdParty/Steamworks/Steamv164`) — older than that if you are on a patched install, and any code you wrote against the bare `SteamAPI_Init()` should move to the `SteamAPI_InitEx` / `SteamGameServer_InitEx` + `SteamErrMsg` form.

### Phase 1 — appid

**During development: 480 (Spacewar).** It is a shared public appid, which has consequences you must design around rather than tolerate:

- `RequestLobbyList` on 480 returns **every Spacewar lobby on Steam**. Your `FindSessions` will be full of strangers. The only automatic discriminator is `BUILDID`, and other UE projects on 480 with the same engine version can collide. This is exactly why `GameIdTag` / `CSGAMEID` is a required advertised key with a matching `Equals` query setting — it becomes `AddRequestLobbyListStringFilter` on `CSGAMEID_s` and filters **server-side**. Do not ship a build with `bFilterByGameIdTag=false`.
- Conversely, your public lobbies are visible and joinable by strangers. Use `FriendsOnly` visibility for anything you do not want walked into.
- Rich-presence localisation keys (`#StatusFormat`) are Spacewar's, so the friends-list text will read wrong. Cosmetic, expected, not a bug to chase.
- No stats/achievements/leaderboards/DLC of yours exist on 480.

**For release:** get a real appid from Steam Partner, then:
1. `SteamDevAppId=<appid>` in `[OnlineSubsystemSteam]` (non-Shipping only).
2. `GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=<appid>")` in `Source/ChronoSpace.Target.cs` — Shipping ignores the ini key entirely and this macro `#define`s to **0** unless you supply it. There is no UBT/ini plumbing for it.
3. `GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDIR=\"chronospace\"")` — defaults to `"unrealtest"`, and is the mandatory `gamedir` filter on the server-browser path. Irrelevant while you are lobby-only, but set it so it is not wrong later.
4. Flip `bRelaunchInSteam=true`.

**Do not set `SteamAppId=`.** Grepping `TEXT("SteamAppId")` across OnlineSubsystemSteam and Runtime/Steam in 5.8 returns nothing. Only `SteamDevAppId` is read. Every tutorial that tells you to set both is pre-5.x.

### Phase 2 — steam_appid.txt

You do not write this file. `WriteSteamAppIdToDisk()` (`SteamSharedModule.cpp:84-118`) reads `SteamDevAppId` and writes it to `FPlatformProcess::BaseDir() + "steam_appid.txt"` right before `SteamAPI_InitEx` (`:353`), and `DeleteSteamAppIdFromDisk()` removes it in `ShutdownModule` (`:167`). The whole block is `#if !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR`. Nothing in UE parses the file — the Steamworks SDK reads it inside `SteamAPI_InitEx` to tell a game launched outside the Steam client which appid it is.

**The repo root already contains a `steam_appid.txt` containing `480`, and it is inert.** `BaseDir()` is the directory of the running executable — `Engine/Binaries/Win64/` for the editor, the packaged `Binaries/Win64/` for a build — never the project root. Delete it or leave it, but do not edit it expecting an effect, and do not conclude Steam is "already set up" because it exists (`.claude/rules/04-multiplayer.md` already flags this file as not indicating the active path).

### Phase 3 — enable and configure

Apply the `.uproject` and `DefaultEngine.ini` changes above. The two that will bite you if skipped:
- `bAutomaticallySetupEIK=False`, or EIK rewrites your `DefaultEngine.ini` on next launch and reverts `DefaultPlatformService`.
- `bUseSteamNetworking=false`, or SteamSockets becomes the process-wide default socket subsystem and breaks the EIK and IP paths.

Then rebuild the game module and commit `Binaries/Win64/UnrealEditor-ChronoSpace.dll` (see the `.uproject` section).

### Phase 4 — testing, and why PIE is useless here

**Steam OSS is force-disabled inside the editor process.** `FOnlineSubsystemSteam::IsEnabled()` is `#if UE_EDITOR bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();` (`OnlineSubsystemSteam.cpp:479-484`), and both socket-subsystem modules skip startup with "Disabled for editor process." PIE-in-editor will **never** exercise the Steam path, no matter what you configure. `FCSOnlineBackend_Steam::IsAvailable()` reports this up front rather than letting you discover it in `CreateSession`.

So the test matrix is:
1. **Standalone from the editor** (`Play → Standalone Game`) or `Launch`, which spawns a separate `-game` process. This is the minimum viable Steam test.
2. **Packaged Development build** — closest to reality, and the only way to exercise packaging/staging of `steam_api64.dll`.
3. Steam client must be **running and logged in**. No Steam client → `SteamAPI_InitEx` fails → `FOnlineSubsystemSteam::Init()` returns false → `IOnlineSubsystem::Get("STEAM")` is null forever → `FCSOnlineBackend_Steam::IsAvailable()` false → the fallback chain picks EIK. Steam client in **offline mode** → subsystem exists but lobby search fails, because `FindLobbies` requires `SteamUser()->BLoggedOn()` (`OnlineSessionAsyncLobbySteam.cpp:963`).
4. **Two Steam accounts on two machines.** Two Steam clients cannot both be signed in on one box. For a same-machine 2-player smoke test, run one instance with `-nosteam` and the EIK backend, and the other on Steam — they will not see each other's sessions, but it verifies each side independently.

Useful launch flags, all non-Shipping only:
- `-CSOnlineBackend=STEAM` — our selector.
- `-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=STEAM` — also moves the AdvancedSessions Blueprint proxies. There is no `-OnlineSubsystem=` switch; this generic form (`ConfigCacheIni.cpp:1716`) is the supported way.
- `-noSTEAM` / `-noEIK` — disables a backend entirely (`OnlineSubsystem.cpp:392-418`). Result is cached in a static on first query, so it cannot change mid-session.
- `-NetDriverOverrides="GameNetDriver,/Script/SteamSockets.SteamSocketsNetDriver,/Script/OnlineSubsystemUtils.IpNetDriver"` — parsed once into a function-local static (`UnrealEngine.cpp:14933`) and compiled out in Shipping. Handy for isolating a transport problem from a session problem.

### Phase 5 — the smoke test, in order

1. Launch Standalone on machine A. `cs.Online.Status` → expect `Backend=Steam`, a valid SteamID64, `UsesLobbies=true`, `GameNetDriver=/Script/SteamSockets.SteamSocketsNetDriver`.
2. `cs.Online.Host 8`. Watch for `LogOnline` create/start success, then the `FCSNetDriverSwitcher` line, then `ServerTravel`.
3. Launch Standalone on machine B, `cs.Online.Find`, then `cs.Online.List`. If it prints "found N, kept 0" you have a filter mismatch (`CSGAMEID` or `BUILDID`); if it prints "found 0" the query never matched (`SEARCH_LOBBIES` missing, or Steam not logged on).
4. `cs.Online.Join 0`. The connect string should be `steam.7656119…:7777`. If `ClientTravel` fails with a URL error, the net driver did not resolve — check `LogNet` for the `IsAvailable()` fallback to `IpNetDriver`.
5. Repeat all of it with `-CSOnlineBackend=EIK` and confirm the connect string is `EOS:…` and the driver is `NetDriverEIK`. Both backends passing the same five steps is the acceptance criterion.

### What is deliberately not set up

`NativePlatformService=Steam` — the "Steam identity → EOS sessions" bridge. EIK supports it first-class (`ToEOS_EExternalCredentialType` maps STEAM to `EOS_ECT_STEAM_SESSION_TICKET`, `UserManagerEOS.cpp:104-110`; `AutoLogin_SteamLogin` produces `"eas_+_EIK_LCT_ExternalAuth_+_EIK_ECT_STEAM_SESSION_TICKET"`, `:1934-1953`). It is the natural *third* configuration once both backends work independently — Steam accounts with EOS infrastructure — and it is worth knowing it exists. But it changes login behaviour under the EIK backend, so it should not be switched on in the same change as the dual-backend work.

## 6. 파일별 상세 스펙

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineTypes.h`

**목적**: All Blueprint-facing enums, structs and dynamic delegates for the online layer. Header-only (UHT handles USTRUCT/UENUM headers with no .cpp). NO #if guards anywhere in this file — see spec.

CRITICAL: this file has ZERO #if guards, deliberately. `ECSOnlineBackend` must contain EIK and Steam unconditionally, because Blueprint assets serialize enum values by name; if the enumerator vanished when a backend is compiled out, every BP that references it would break on the cutover. The enum is data, not backend code.

```cpp
#pragma once
#include "CoreMinimal.h"
#include "CSOnlineTypes.generated.h"

UENUM(BlueprintType)
enum class ECSOnlineBackend : uint8
{
    None  = 0 UMETA(DisplayName = "None (unresolved)"),
    EIK   = 1 UMETA(DisplayName = "EOS / EIK"),
    Steam = 2 UMETA(DisplayName = "Steam"),
    Null  = 3 UMETA(DisplayName = "Null (LAN / offline)")
};

UENUM(BlueprintType)
enum class ECSOnlineOpResult : uint8
{
    Success = 0, Failed, Busy, NoBackend, NotLoggedIn,
    InvalidSession, AlreadyInSession, SessionFull,
    VersionMismatch, Cancelled, Timeout
};

UENUM(BlueprintType)
enum class ECSSessionVisibility : uint8
{
    Public = 0, FriendsOnly, InviteOnly
};

UENUM(BlueprintType)
enum class ECSOnlineRole : uint8
{
    Client = 0, ListenHost, DedicatedServer
};
```

`USTRUCT(BlueprintType) struct CHRONOSPACE_API FCSHostSessionParams` — every member `UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Session")` with the given default:
- `int32 MaxPlayers = 8;` — meta `(ClampMin="1", ClampMax="64")`. Must be >= 1: `FindEOSSession` unconditionally injects `NumPublicConnections >= 1` (`OnlineSessionEOS.cpp:2415`), so a 0-public session is invisible on the EIK Sessions path.
- `ECSSessionVisibility Visibility = ECSSessionVisibility::Public;`
- `bool bAllowJoinInProgress = true;`
- `bool bIsLANMatch = false;`
- `FString SessionDisplayName;` — advertised under key `CSNAME`; empty falls back to the local nickname.
- `TSoftObjectPtr<UWorld> TravelLevel;` — null falls back to `UCSOnlineSettings::DefaultHostLevel`.
- `FString ExtraTravelOptions;` — appended verbatim, e.g. `?SpawnTag=Start`. Must NOT contain `?listen` (the layer adds it); `HostSession` strips it defensively and logs a Warning.
- `FString GameIdTagOverride;` — empty uses `UCSOnlineSettings::GameIdTag`.

`USTRUCT(BlueprintType) struct CHRONOSPACE_API FCSFindSessionsParams`:
- `int32 MaxResults = 50;` meta `(ClampMin="1")`
- `bool bLANQuery = false;`
- `bool bFilterByGameIdTag = true;` — leave true. On Steam appid 480 `RequestLobbyList` returns every Spacewar lobby on Steam; this is the only server-side discriminator you get.
- `bool bFilterByBuildId = true;` — client-side `BuildUniqueId` check. Needed because the EIK **lobby** path applies no bucket filter (only the Sessions path does, `OnlineSessionEOS.cpp:2417`).

`USTRUCT(BlueprintType) struct CHRONOSPACE_API FCSSessionSearchResult` — all `UPROPERTY(BlueprintReadOnly, Category="Session")`. This is a *view*, never the join token; joining goes by index into the live search.
- `int32 ResultIndex = INDEX_NONE;`
- `int32 SearchGeneration = 0;` — see the staleness rule below.
- `ECSOnlineBackend Backend = ECSOnlineBackend::None;`
- `FString SessionDisplayName;`, `FString OwningPlayerName;`, `FString SessionIdString;`, `FString MapName;`
- `int32 PingInMs = -1;`, `int32 MaxPlayers = 0;`, `int32 OpenSlots = 0;`, `int32 CurrentPlayers = 0;`
- `bool bIsLAN = false;`, `bool bAllowJoinInProgress = false;`, `bool bIsFull = false;`

WHY INDEX AND GENERATION, NOT A COPIED `FOnlineSessionSearchResult`: EIK's `JoinLobbySession` requires the lobby to still be present in `LobbySearchResultsCache`, keyed by session-id string (`OnlineSessionEOS.cpp:4068-4076`), and that cache is wiped at the start of every `StartLobbySearch` (`:4526`). A `FOnlineSessionSearchResult` copied out of a superseded search therefore cannot be joined. `SearchGeneration` is compared against the subsystem's counter on every join and a mismatch returns `ECSOnlineOpResult::InvalidSession` with a clear log line instead of failing deep inside EOS.

Dynamic multicast delegates (all at file scope, after the structs):
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnLoginCompleteBP, bool, bWasSuccessful, const FString&, PlayerNickname, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnHostCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnFindSessionsCompleteBP, ECSOnlineOpResult, Result, const TArray<FCSSessionSearchResult>&, Results, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnJoinSessionCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnLeaveSessionCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnBackendChangedBP, ECSOnlineBackend, OldBackend, ECSOnlineBackend, NewBackend);
```
`const TArray<FCSSessionSearchResult>&` as a dynamic-delegate param is legal — the engine's own `FBlueprintFindSessionsResultDelegate` (`FindSessionsCallbackProxy.h:19`) does exactly this.

---

### `C:/Git/WindUp/Source/ChronoSpace/Settings/CSOnlineSettings.h`

**목적**: UDeveloperSettings exposing the whole online layer to Project Settings and DefaultEngine.ini. Lives in Settings/ (developer settings) per the documented Setting-vs-Settings split in CLAUDE.md.

`UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "ChronoSpace Online")) class CHRONOSPACE_API UCSOnlineSettings : public UDeveloperSettings`. Mirrors the existing `UCSStageDataSettings` pattern (`CSStageDataSettings.h:76`) except `Config = Engine` rather than `Game`, because the backend choice must sit in the same file as `[OnlineSubsystem] DefaultPlatformService` and be readable at engine-startup time.

Includes: `Engine/DeveloperSettings.h`, `Engine/World.h`, `Subsystem/Online/CSOnlineTypes.h`.

All properties `UPROPERTY(EditAnywhere, Config, Category = "<X>")`.

Category "Backend":
- `ECSOnlineBackend DefaultBackend = ECSOnlineBackend::EIK;`
- `TArray<ECSOnlineBackend> BackendFallbackOrder;` — ctor seeds `{EIK, Steam, Null}`.
- `bool bAllowRuntimeBackendSwitch = true;`
- `bool bAutoLoginOnInitialize = false;` — leave false. Turning it true resurrects the behaviour `CSEIKSubsystem.cpp:18` has commented out and starts a network op at GameInstance construction.
- `FName EIKSubsystemName = FName(TEXT("EIK"));` — EIK registers this as a bare string literal (`OnlineSubsystemModuleEIK.cpp:66`); there is no macro constant in the plugin, so it is surfaced here rather than hardcoded in the adapter.
- `FName SteamSubsystemName = FName(TEXT("STEAM"));` — defaults to `STEAM_SUBSYSTEM`; exposed only for inspection/override.

Category "Session":
- `TSoftObjectPtr<UWorld> DefaultHostLevel;` — replaces the hardcoded `/Game/02_Map/L_StageSize` at `CSEIKSubsystem.cpp:115` and `:164`.
- `int32 DefaultMaxPlayers = 8;` meta `(ClampMin="1", ClampMax="64")` — replaces the hardcoded `2` at `CSEIKSubsystem.cpp:110`.
- `FString GameIdTag = TEXT("ChronoSpace");`
- `FName GameIdTagKey = FName(TEXT("CSGAMEID"));`
- `FName SessionNameKey = FName(TEXT("CSNAME"));`
- `float OperationTimeoutSeconds = 30.0f;` meta `(ClampMin="5.0")`

Category "Session|EIK":
- `bool bEIKUseLobbies = false;` — **false keeps today's behaviour** (EOS Sessions; the project currently creates without `bUseLobbiesIfAvailable` and searches without `SEARCH_LOBBIES`, which is consistent). True switches to EOS Lobbies, which buys host migration and the RTC voice room but changes join routing. Whatever this is, it drives BOTH create and find through `FCSOnlineBackend_EIK::UsesLobbies()`.

Category "Session|Steam":
- `bool bSteamUseLobbies = true;` — meta `(EditCondition="false", EditConditionHides=false)` so it shows but cannot be edited, with a tooltip. Must stay true: the non-lobby Steam path is the server browser, which needs `IsSteamServerAvailable()` i.e. `bInitServerOnClient=true` or a dedicated server. `FCSOnlineBackend_Steam::UsesLobbies()` returns `true` unconditionally and logs an Error if this is ever false, rather than honouring it.

Category "NetDriver":
- `bool bManageNetDriverDefinitions = true;`
- `bool bManageNetDriverInPIE = false;`
- `FString EIKNetDriverClassName = TEXT("/Script/OnlineSubsystemEIK.NetDriverEIK");`
- `FString SteamNetDriverClassName = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");` — **not** `/Script/OnlineSubsystemSteam.SteamNetDriver`, which no longer exists in 5.8; and not `/Script/SocketSubsystemSteamIP.SteamNetDriver`, which is the non-NAT-punch legacy path.
- `FString FallbackNetDriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");`

Statics:
- `static const UCSOnlineSettings& Get();`
- `ECSOnlineBackend ResolveStartupBackend() const;`
- `static ECSOnlineBackend ParseBackendName(const FString& In, bool& bOutOk);`
- `static const TCHAR* LexToString(ECSOnlineBackend);`
- `void WritePreferredBackend(ECSOnlineBackend) const;` — writes `GGameUserSettingsIni`, mirroring `UCSLoginIdLibrary`'s GConfig+Flush idiom (`CSLoginIdLibrary.cpp:22-27`).

---

### `C:/Git/WindUp/Source/ChronoSpace/Settings/CSOnlineSettings.cpp`

**목적**: Implements Get(), the backend-resolution precedence chain, and the enum<->string helpers.

`Get()` is `*GetDefault<UCSOnlineSettings>()`.

`ResolveStartupBackend()` precedence, first hit wins, each logged at `LogCS, Log` naming the source so a mis-selection is one grep away:
1. `FParse::Value(FCommandLine::Get(), TEXT("CSOnlineBackend="), Str)` → `ParseBackendName`. Invalid string → `LogCS, Warning` and fall through (do not silently ignore).
2. `GConfig->GetString(TEXT("/Script/ChronoSpace.CSOnlineSettings"), TEXT("PreferredBackend"), Str, GGameUserSettingsIni)`.
3. `DefaultBackend` if != `None`.
4. `BackendFallbackOrder[0]` if non-empty.
5. `ECSOnlineBackend::Null`.

NOTE this function does **not** consult availability — it returns an *intent*. `UCSOnlineSessionSubsystem::ResolveBackend` walks `BackendFallbackOrder` for actual availability. Keeping intent and availability separate is what lets the log say "requested Steam, Steam unavailable (client not running), fell back to EIK" instead of just reporting EIK.

`ParseBackendName` is case-insensitive and accepts the aliases `EOS`→EIK, `NONE`/`OFFLINE`→Null, so `-CSOnlineBackend=EOS` works for anyone who types it that way.

`WritePreferredBackend` does `GConfig->SetString(...); GConfig->Flush(false, GGameUserSettingsIni);`.

Ctor seeds `BackendFallbackOrder` only when empty, so a config-supplied array is never appended to.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend.h`

**목적**: The adapter interface every backend implements. Pure virtual, non-UObject, no #if.

Includes: `CoreMinimal.h`, `OnlineSubsystem.h`, `OnlineSessionSettings.h`, `Interfaces/OnlineIdentityInterface.h`, `Interfaces/OnlineSessionInterface.h`, `Subsystem/Online/CSOnlineTypes.h`. Forward-declares `class UCSOnlineSettings;`.

```cpp
class CHRONOSPACE_API ICSOnlineBackend
{
public:
    virtual ~ICSOnlineBackend() = default;

    // --- identity of the adapter -------------------------------------
    virtual ECSOnlineBackend GetBackendId() const = 0;
    virtual FName            GetSubsystemName() const = 0;
    virtual FText            GetDisplayName() const = 0;

    // Module loadable, OSS instance creatable, prerequisites met.
    // Must be cheap and safe to call at any time, including before login.
    virtual bool IsAvailable() const = 0;

    // Human-readable reason IsAvailable() is false. Empty when available.
    virtual FString GetUnavailableReason() const = 0;

    // --- transport ----------------------------------------------------
    virtual FString GetNetDriverClassName() const = 0;

    // --- THE invariant ------------------------------------------------
    // Single source of truth for lobbies-vs-sessions. Read by BOTH
    // FillCreateSettings and FillSearchSettings. Never branch on anything
    // else for this decision.
    virtual bool UsesLobbies() const = 0;

    // --- identity -----------------------------------------------------
    virtual bool NeedsExplicitLogin() const = 0;
    virtual bool BuildLoginCredentials(int32 LocalUserNum,
                                       FOnlineAccountCredentials& OutCreds) const = 0;

    // --- session shaping ---------------------------------------------
    virtual void FillCreateSettings(FOnlineSessionSettings& InOutSettings,
                                    const FCSHostSessionParams& Params,
                                    const UCSOnlineSettings& Config) const = 0;

    virtual void FillSearchSettings(FOnlineSessionSearch& InOutSearch,
                                    const FCSFindSessionsParams& Params,
                                    const UCSOnlineSettings& Config) const = 0;

    virtual bool ShouldAcceptSearchResult(const FOnlineSessionSearchResult& Result,
                                          const FCSFindSessionsParams& Params,
                                          const UCSOnlineSettings& Config) const = 0;

    // --- travel-URL hooks (default no-op) ----------------------------
    virtual void DecorateListenURL(FString& InOutURL) const {}
    virtual void DecorateClientTravelURL(FString& InOutURL) const {}

    // --- dedicated-server hook, default = unsupported -----------------
    // Present so the future dedicated track has a seam. Returning false
    // makes UCSOnlineSessionSubsystem refuse to host as a dedicated server
    // on this backend with a clear message rather than half-working.
    virtual bool BuildDedicatedServerCredentials(FOnlineAccountCredentials& OutCreds) const { return false; }

    // --- non-virtual conveniences (implemented in the .cpp) ----------
    IOnlineSubsystem*  GetSubsystem() const;
    IOnlineSessionPtr  GetSessionInterface() const;
    IOnlineIdentityPtr GetIdentityInterface() const;
    bool               IsLoggedIn(int32 LocalUserNum = 0) const;

protected:
    // Shared helpers every adapter uses, so the advertised-key spelling
    // cannot drift between backends.
    static void ApplyCommonCreateSettings(FOnlineSessionSettings& InOut,
                                          const FCSHostSessionParams& Params,
                                          const UCSOnlineSettings& Config);
    static bool MatchesGameIdTag(const FOnlineSessionSearchResult& Result,
                                 const UCSOnlineSettings& Config);
    static bool MatchesBuildId(const FOnlineSessionSearchResult& Result);
};
```

`GetUnavailableReason()` exists because both backends fail *silently* by default — `LoadDefaultSubsystem` falls back to NULL with the failure only at LogOnline Verbose (`OnlineSubsystemModule.cpp:216-221`), and `FOnlineSubsystemSteam::Init()` returns false with just "Could not set up the steam environment!" when `SteamDevAppId` is missing. Surfacing the reason into the UI/log is the difference between a five-minute and a five-hour diagnosis.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend.cpp`

**목적**: Non-virtual convenience methods and the shared static helpers. No #if.

`GetSubsystem()` → `IOnlineSubsystem::Get(GetSubsystemName())`. Never `IOnlineSubsystem::Get()` with no argument anywhere in this codebase — the whole design depends on addressing backends explicitly so the default-service setting is irrelevant to us.

`GetSessionInterface()` / `GetIdentityInterface()` null-check the subsystem and return an empty `TSharedPtr` rather than dereferencing. This is the specific fix for `CSEIKSubsystem.cpp:122-124, 147-149, 200-202, 251-253`, which dereference unconditionally and reliably crash when Steam is not running.

`IsLoggedIn(N)` → identity valid && `GetLoginStatus(N) == ELoginStatus::LoggedIn`.

`ApplyCommonCreateSettings` sets only what is genuinely identical across backends, and nothing that either backend has a hard rule about:
- `InOut.NumPublicConnections = FMath::Max(1, Params.MaxPlayers);`
- `InOut.NumPrivateConnections = 0;`
- `InOut.bAllowJoinInProgress = Params.bAllowJoinInProgress;`
- `InOut.bIsLANMatch = Params.bIsLANMatch;`
- `InOut.bIsDedicated = false;`
- `InOut.bAllowInvites = true;`
- `InOut.bAntiCheatProtected = false;` (`Docs/DedicatedServer_Migration.md:531` confirms no anti-cheat is wired)
- `InOut.bUsesStats = false;`
- `InOut.Set(Config.GameIdTagKey, EffectiveGameIdTag, EOnlineDataAdvertisementType::ViaOnlineService);`
- `InOut.Set(Config.SessionNameKey, Params.SessionDisplayName, EOnlineDataAdvertisementType::ViaOnlineService);`
- `InOut.Set(SETTING_MAPNAME, MapShortName, EOnlineDataAdvertisementType::ViaOnlineService);`

`AdvertisementType` must be `>= ViaOnlineService` or the key never reaches the backend at all — Steam drops anything lower (`OnlineSessionAsyncLobbySteam.cpp:130`) and EIK only forwards `>= ViaOnlineService` (`OnlineSessionEOS.cpp:1313`). Do not use `ViaOnlineServiceAndPing` for the tag: Steam's `SessionKeyToSteamKey` mangles by *type* not advertisement mode so it is harmless there, but `ViaOnlineService` is the honest minimum and matches what both filters compare.

`MatchesGameIdTag` reads the key back off `Result.Session.SessionSettings` and string-compares. Returns true when `bFilterByGameIdTag` is false.

`MatchesBuildId` compares `Result.Session.SessionSettings.BuildUniqueId` against `GetBuildUniqueId()`. Steam already discards mismatches server-side (`OnlineSessionAsyncLobbySteam.cpp:405-413`) and the EIK Sessions path filters by bucket, but the EIK *lobby* path filters by neither — so this must exist client-side.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackendRegistry.h`

**목적**: Lazy singleton owning one instance of each compiled-in adapter. The only place in the codebase that names both adapters.

```cpp
class CHRONOSPACE_API FCSOnlineBackendRegistry
{
public:
    static FCSOnlineBackendRegistry& Get();

    TSharedPtr<ICSOnlineBackend> Find(ECSOnlineBackend Id) const;

    // Walks PreferenceOrder and returns the first adapter whose
    // IsAvailable() is true. Never returns null: falls through to Null.
    TSharedRef<ICSOnlineBackend> FindFirstAvailable(
        TArrayView<const ECSOnlineBackend> PreferenceOrder) const;

    void GetCompiledBackends(TArray<ECSOnlineBackend>& Out) const;
    void GetAvailableBackends(TArray<ECSOnlineBackend>& Out) const;

    static bool IsBackendCompiledIn(ECSOnlineBackend Id);

private:
    FCSOnlineBackendRegistry();
    void RegisterBackend(const TSharedRef<ICSOnlineBackend>& Backend);

    TMap<ECSOnlineBackend, TSharedRef<ICSOnlineBackend>> Backends;
    TArray<ECSOnlineBackend> RegistrationOrder;
};
```

No copy/move; `FCSOnlineBackendRegistry(const FCSOnlineBackendRegistry&) = delete;`.

`FindFirstAvailable` returning `TSharedRef` (not Ptr) is deliberate — the Null adapter is always compiled and always available, so the façade never has a null-backend code path to get wrong.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackendRegistry.cpp`

**목적**: Registry construction. THIS IS THE ONLY FILE WITH #if THAT REFERENCES BOTH ADAPTERS.

Guarded includes at the top — the adapter headers are never included unguarded anywhere:
```cpp
#include "Subsystem/Online/CSOnlineBackendRegistry.h"
#include "Subsystem/Online/CSOnlineBackend_Null.h"
#include "Settings/CSOnlineSettings.h"
#include "ChronoSpace.h"

#if CS_WITH_EIK
#include "Subsystem/Online/CSOnlineBackend_EIK.h"
#endif
#if CS_WITH_STEAM
#include "Subsystem/Online/CSOnlineBackend_Steam.h"
#endif
```

```cpp
FCSOnlineBackendRegistry& FCSOnlineBackendRegistry::Get()
{
    static FCSOnlineBackendRegistry Instance;   // C++11 magic static: lazy + thread-safe
    return Instance;
}

FCSOnlineBackendRegistry::FCSOnlineBackendRegistry()
{
#if CS_WITH_EIK
    RegisterBackend(MakeShared<FCSOnlineBackend_EIK>());
#endif
#if CS_WITH_STEAM
    RegisterBackend(MakeShared<FCSOnlineBackend_Steam>());
#endif
    RegisterBackend(MakeShared<FCSOnlineBackend_Null>());   // always last, always present

    UE_LOG(LogCS, Log, TEXT("[Online] Registry built: CS_WITH_EIK=%d CS_WITH_STEAM=%d CS_WITH_STEAMSOCKETS=%d, %d adapter(s)"),
           CS_WITH_EIK, CS_WITH_STEAM, CS_WITH_STEAMSOCKETS, Backends.Num());
}
```

WHY A LAZY SINGLETON AND NOT A MODULE STARTUP HOOK: `ChronoSpace.cpp:7` uses `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ...)` — there is no custom module class to hang a `StartupModule` on, and adding one just for this is more surface than the problem needs. A function-local static is lazy, deterministic, thread-safe since C++11, and has no static-initialisation-order hazard. First touch is `UCSOnlineSessionSubsystem::Initialize`.

`IsBackendCompiledIn` is a `switch` returning the literal macro value per case — so UI can grey out a backend that was compiled out, rather than offering it and failing.

`FindFirstAvailable` logs at `LogCS, Verbose` for each candidate it rejects, including `GetUnavailableReason()`, then `LogCS, Warning` if it had to fall past the first choice.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_Null.h`

**목적**: Always-compiled fallback adapter over NULL_SUBSYSTEM. Guarantees the façade always has a valid backend.

NO #if guard — this file always compiles. `class FCSOnlineBackend_Null final : public ICSOnlineBackend`.

- `GetBackendId()` → `ECSOnlineBackend::Null`
- `GetSubsystemName()` → `NULL_SUBSYSTEM` (from `OnlineSubsystemNames.h`, generic plugin)
- `GetDisplayName()` → `NSLOCTEXT("ChronoSpace", "Backend_Null", "Offline / LAN")`
- `IsAvailable()` → `GetSubsystem() != nullptr` (the Null OSS is always constructible)
- `GetNetDriverClassName()` → `UCSOnlineSettings::Get().FallbackNetDriverClassName`
- `UsesLobbies()` → `false`
- `NeedsExplicitLogin()` → `false`
- `BuildLoginCredentials` → returns `false` (nothing to do)
- `FillCreateSettings` → `ApplyCommonCreateSettings`, then forces `bIsLANMatch = true`, `bShouldAdvertise = true`, `bUsesPresence = false`, `bUseLobbiesIfAvailable = false`. `bShouldAdvertise` matters here and only here: it is the one path the engine actually reads it on (`CreateLANSession`).
- `FillSearchSettings` → `bIsLanQuery = true`, `MaxSearchResults = Params.MaxResults`, no query keys.
- `ShouldAcceptSearchResult` → `MatchesBuildId` only.

Why it earns its place: it makes "no backend" unrepresentable in the façade, gives PIE and LAN a working path (Steam is force-disabled in-editor and EIK P2P is awkward there), and gives the fallback chain a guaranteed terminator.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_Null.cpp`

**목적**: Implementation of the Null adapter.

Straightforward. Includes `Online/OnlineSessionNames.h` for `SETTING_MAPNAME`, `OnlineSubsystemNames.h` for `NULL_SUBSYSTEM`. No `#if`.

One subtlety: `FillCreateSettings` must set `bUsesPresence = false` AND `bUseLobbiesIfAvailable = false` together. The Null OSS does not enforce the Steam equality rule, but keeping them consistent means a future swap of this adapter's target does not surprise anyone.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_EIK.h`

**목적**: EIK/EOS adapter. Entire file body inside #if CS_WITH_EIK.

File shape — the guard wraps *everything* after the includes, so with `CS_WITH_EIK=0` the header is empty and the type does not exist:
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Subsystem/Online/CSOnlineBackend.h"

#if CS_WITH_EIK

class FCSOnlineBackend_EIK final : public ICSOnlineBackend
{
public:
    virtual ECSOnlineBackend GetBackendId() const override { return ECSOnlineBackend::EIK; }
    virtual FName   GetSubsystemName() const override;
    virtual FText   GetDisplayName() const override;
    virtual bool    IsAvailable() const override;
    virtual FString GetUnavailableReason() const override;
    virtual FString GetNetDriverClassName() const override;
    virtual bool    UsesLobbies() const override;
    virtual bool    NeedsExplicitLogin() const override { return true; }
    virtual bool    BuildLoginCredentials(int32 LocalUserNum, FOnlineAccountCredentials& OutCreds) const override;
    virtual void    FillCreateSettings(FOnlineSessionSettings&, const FCSHostSessionParams&, const UCSOnlineSettings&) const override;
    virtual void    FillSearchSettings(FOnlineSessionSearch&, const FCSFindSessionsParams&, const UCSOnlineSettings&) const override;
    virtual bool    ShouldAcceptSearchResult(const FOnlineSessionSearchResult&, const FCSFindSessionsParams&, const UCSOnlineSettings&) const override;

private:
    static const TCHAR* GetDeviceIdCredentialType();
};

#endif // CS_WITH_EIK
```

`GetSubsystemName()` returns `UCSOnlineSettings::Get().EIKSubsystemName` (defaulting to `FName(TEXT("EIK"))`). There is genuinely no macro to use — grep for `EIK_SUBSYSTEM`/`EOS_SUBSYSTEM` in the plugin returns nothing; `OnlineSubsystemModuleEIK.cpp:66` registers a bare `"EIK"` string literal.

`GetDeviceIdCredentialType()` returns the exact literal `TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN")`. Add a comment stating this is compared with plain `FString ==` at `UserManagerEOS.cpp:1077` to decide whether to auto-`CreateDeviceID` on first launch, so ANY deviation (including the fully-qualified `EEIK_EExternalCredentialType::EIK_ECT_...` form that EIK's own Blueprint node produces at `EIK_Login_AsyncFunction.cpp:72`) makes first launch on a fresh machine return `EOS_NotFound` and fail instead of provisioning the device id. This is the single most fragile string in the system; it must not be reconstructed from an enum.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_EIK.cpp`

**목적**: EIK adapter implementation. Whole body inside #if CS_WITH_EIK.

Same guard shape; `#include "Subsystem/Online/CSOnlineBackend_EIK.h"` then `#if CS_WITH_EIK` around the rest, plus `Online/OnlineSessionNames.h`, `Settings/CSOnlineSettings.h`, `Common/CSLoginIdLibrary.h`, `ChronoSpace.h`.

Empty-TU note: when `CS_WITH_EIK=0` this compiles to an empty object. Harmless for a DLL module, but MSVC can emit LNK4221. If it shows up, add inside the `#else`: `namespace { [[maybe_unused]] constexpr int CSOnlineBackendEIK_TUAnchor = 0; }`.

**IsAvailable()**: subsystem non-null && session interface valid && identity interface valid. Do NOT require logged-in — the façade drives login. `GetUnavailableReason()` distinguishes "OnlineSubsystemEIK module not loaded", "[OnlineSubsystemEIK] bEnabled=false", and "session/identity interface missing".

**BuildLoginCredentials(LocalUserNum, OutCreds)**:
```cpp
// EIK hardcodes LocalUserNum 0 on the connect-interface path
// (UserManagerEOS.cpp:660, :691) and the first-run device-id fallback
// indexes LocalUserNumToLastLoginCredentials[0] (:1077). Anything else
// silently will not work.
if (LocalUserNum != 0) { UE_LOG(LogCS, Warning, ...); return false; }
OutCreds.Type  = GetDeviceIdCredentialType();
OutCreds.Id    = UCSLoginIdLibrary::GetPersistentDeviceLoginId();
OutCreds.Token = FString();
return true;
```
Using `UCSLoginIdLibrary` (`CSLoginIdLibrary.cpp:10-30`) rather than `FGuid::NewGuid()` is a real behaviour fix: `CSEIKSubsystem.cpp:58` mints a fresh device id every launch, so the EOS account is never stable across sessions. The library already persists to `GGameUserSettingsIni`, honours `-LoginId=`, and clamps to 32 chars — and it is what the live Blueprints already use, so C++ and BP finally agree on one identity.

**UsesLobbies()** → `UCSOnlineSettings::Get().bEIKUseLobbies`.

**FillCreateSettings**: call `ApplyCommonCreateSettings` first, then:
- `bUseLobbiesIfAvailable = UsesLobbies();`
- `bUseLobbiesVoiceChatIfAvailable = false;`
- `bUsesPresence = true;`
- `bAllowJoinViaPresence = (Params.Visibility != ECSSessionVisibility::InviteOnly);`
- `bAllowJoinViaPresenceFriendsOnly = (Params.Visibility == ECSSessionVisibility::FriendsOnly);`
- `bShouldAdvertise = true;` — set truthfully even though EIK reads it only on the LAN path (`OnlineSessionEOS.cpp:1210`); visibility online comes from PermissionLevel.
- InviteOnly special case, which must be explicit because EIK derives PermissionLevel from connection counts (`:1227`, `:3903`): when `Visibility == InviteOnly`, move the capacity to private — `NumPublicConnections = 0; NumPrivateConnections = FMath::Max(1, Params.MaxPlayers);` — and `UE_LOG(LogCS, Log, ...)` that the session is intentionally undiscoverable via FindSessions, because `FindEOSSession` injects `NumPublicConnections >= 1` (`:2415`).

**FillSearchSettings**:
- `bIsLanQuery = Params.bLANQuery; MaxSearchResults = Params.MaxResults;`
- if `UsesLobbies()` → `QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);` (`SEARCH_LOBBIES` == `FName("LOBBYSEARCH")`, `OnlineSessionNames.h:151`)
- else → `QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);` with the comment that `SEARCH_PRESENCE` **does not exist in UE 5.8** (verified: zero hits in `OnlineSessionNames.h`) and EIK advertises the raw literal at `OnlineSessionEOS.cpp:1318`.
- if `Params.bFilterByGameIdTag` → `QuerySettings.Set(Config.GameIdTagKey, EffectiveTag, EOnlineComparisonOp::Equals);`
- Do NOT set `SEARCH_MINSLOTSAVAILABLE`, `SEARCH_EMPTY_SERVERS_ONLY`, `SEARCH_KEYWORDS` — EIK forwards them verbatim as EOS attribute comparisons that nothing advertises, so they match nothing.

**ShouldAcceptSearchResult**: `MatchesBuildId` (mandatory on the lobby path, which applies no bucket filter) `&& MatchesGameIdTag`. Also reject `Result.Session.SessionSettings.bIsDedicated` while the dedicated track is out of scope.

**GetNetDriverClassName()** → `Config.EIKNetDriverClassName`. Add a comment that the resolved connect string is only usable when this driver is live with `bIsUsingP2PSockets=true`; with the flag false `CreateEOSSession` silently advertises `127.0.0.1` (`OnlineSessionEOS.cpp:1512`) and every join fails at travel time with no error at create time.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_Steam.h`

**목적**: Steam adapter. Entire file body inside #if CS_WITH_STEAM.

Identical shape to the EIK header, `#if CS_WITH_STEAM`, `class FCSOnlineBackend_Steam final : public ICSOnlineBackend`.

- `GetBackendId()` → `ECSOnlineBackend::Steam`
- `GetSubsystemName()` → `UCSOnlineSettings::Get().SteamSubsystemName` (default `STEAM_SUBSYSTEM`)
- `NeedsExplicitLogin()` → `false`
- `UsesLobbies()` → `true` (unconditional; see .cpp)
- adds `private: static bool IsSteamUsableInThisProcess();`

Includes only `OnlineSubsystemNames.h` for `STEAM_SUBSYSTEM` — that header ships with the *generic* OnlineSubsystem plugin (`OnlineSubsystemNames.h:53-54`), not the Steam one, so this adapter has no link-time coupling to `OnlineSubsystemSteam` whatsoever. `CS_WITH_STEAM` is a policy guard, not a link guard.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineBackend_Steam.cpp`

**목적**: Steam adapter implementation. Whole body inside #if CS_WITH_STEAM.

**IsSteamUsableInThisProcess()**:
```cpp
#if WITH_EDITOR
    // FOnlineSubsystemSteam::IsEnabled() is
    //   #if UE_EDITOR bEnableSteam = IsRunningDedicatedServer() || IsRunningGame();
    // (OnlineSubsystemSteam.cpp:479-484), and both socket-subsystem modules
    // log "Disabled for editor process." So PIE-in-editor can never exercise
    // Steam. Report unavailable up front instead of failing deep in CreateSession.
    if (!IsRunningDedicatedServer() && !IsRunningGame()) { return false; }
#endif
    return true;
```

**IsAvailable()** → `IsSteamUsableInThisProcess() && GetSubsystem() != nullptr && GetSessionInterface().IsValid() && GetIdentityInterface().IsValid()`.

**GetUnavailableReason()** must distinguish the four failure modes, because every one of them is silent by default:
- editor process → "Steam is disabled inside the editor process; use Standalone or a packaged build."
- subsystem null → "Steam OSS not created. Check: Steam client running and logged in; [OnlineSubsystemSteam] bEnabled=true; SteamDevAppId set (missing SteamDevAppId makes FOnlineSubsystemSteam::Init() return false outright); steam_api64.dll loadable (AreSteamDllsLoaded gates factory registration at OnlineSubsystemModuleSteam.cpp:82)."
- `-nosteam` on the command line.
- interfaces missing.

**BuildLoginCredentials** → returns `false`; Steam needs none. `FOnlineIdentitySteam::Login` ignores credentials entirely and just checks `BLoggedOn()` (`OnlineIdentityInterfaceSteam.cpp:33`), and `GetUniquePlayerId(0)` is valid before any Login call (`:113`). The façade still calls `AutoLogin(0)` to normalise the flow and get the delegate — see the façade spec for the already-logged-in short circuit.

**UsesLobbies()**:
```cpp
if (!UCSOnlineSettings::Get().bSteamUseLobbies)
{
    UE_LOG(LogCS, Error, TEXT("[Online] bSteamUseLobbies=false is not supported; the non-lobby Steam path is the server browser and needs the Steam game-server API. Forcing lobbies."));
}
return true;
```

**FillCreateSettings** — `ApplyCommonCreateSettings` first, then:
```cpp
// HARD REQUIREMENT: FOnlineSessionSteam::CreateSession fires
// OnCreateSessionComplete(false) and returns before doing anything when
// bUsesPresence != bUseLobbiesIfAvailable (OnlineSessionInterfaceSteam.cpp:229-236),
// and JoinSession repeats the check at :861. EIK does not enforce this,
// so shared settings code breaks on Steam only.
InOut.bUsesPresence            = true;
InOut.bUseLobbiesIfAvailable   = true;
```
Then visibility, mapped to `BuildLobbyType` (`OnlineSessionAsyncLobbySteam.cpp:70-96`) — this is the one place Steam genuinely reads `bShouldAdvertise`:
- Public → `bShouldAdvertise=true; bAllowJoinViaPresence=true; bAllowJoinViaPresenceFriendsOnly=false;` → `k_ELobbyTypePublic`
- FriendsOnly → `bShouldAdvertise=true; bAllowJoinViaPresenceFriendsOnly=true;` → `k_ELobbyTypeFriendsOnly`
- InviteOnly → `bShouldAdvertise=false;` → `k_ELobbyTypePrivate`
Also `bUseLobbiesVoiceChatIfAvailable = false;`.

Note in a comment that `NumOpenPublicConnections` becomes `NumPublicConnections - 1` for non-dedicated (`OnlineSessionInterfaceSteam.cpp:249`) — the host consumes a slot — so `MaxPlayers=8` yields 7 joinable, which is what the UI should say.

**FillSearchSettings**:
- `bIsLanQuery = Params.bLANQuery; MaxSearchResults = Params.MaxResults;`
- `QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);` — mandatory; without it `FindInternetSession` routes to the server browser (`OnlineSessionInterfaceSteam.cpp:774`) and returns nothing for lobby hosts.
- `if (Params.bFilterByGameIdTag) QuerySettings.Set(Config.GameIdTagKey, EffectiveTag, EOnlineComparisonOp::Equals);` — becomes `AddRequestLobbyListStringFilter` on `CSGAMEID_s`, filtering **server-side**. On appid 480 this is the only thing standing between you and every Spacewar lobby on Steam.
- **Do NOT set `PRESENCESEARCH`.** A bool is neither Int32 nor String/Float, so `CreateQuery` warns "Unable to set search parameter" and drops it (`OnlineSessionAsyncLobbySteam.cpp:830-916`). Add the comment so nobody "fixes" the asymmetry with EIK later.
- **Do NOT set `SETTING_MAPNAME`, `SEARCH_DEDICATED_ONLY`, `SEARCH_EMPTY_SERVERS_ONLY`, `SEARCH_SECURE_SERVERS_ONLY`** — silently dropped from lobby filters at `:853`.

**ShouldAcceptSearchResult** → `MatchesBuildId && MatchesGameIdTag`. Belt and braces on top of the server-side filter.

**GetNetDriverClassName()** → `Config.SteamNetDriverClassName`, with a comment that `/Script/OnlineSubsystemSteam.SteamNetDriver` is dead in 5.8 (classes moved to `SocketSubsystemSteamIP`, no CoreRedirect, `BaseEngine.ini:2521` still ships the stale section) and resolves to nothing, silently falling back to IpNetDriver.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSNetDriverSwitcher.h`

**목적**: Static helper that retargets the GameNetDriver definition at runtime. Pure strings — no #if, no backend headers.

```cpp
class CHRONOSPACE_API FCSNetDriverSwitcher
{
public:
    // Idempotent. Snapshots the shipped GameNetDriver entry so RestoreDefaults
    // can undo us. Called from UCSOnlineSessionSubsystem::Initialize.
    static void CaptureDefaults();

    static bool ApplyForBackend(const ICSOnlineBackend& Backend, const UWorld* World, FString& OutError);
    static bool ApplyClassName(const FString& DriverClassName,
                               const FString& FallbackClassName,
                               const UWorld* World, FString& OutError);
    static void RestoreDefaults();

    static bool IsSafeToSwitch(const UWorld* World);
    static bool ShouldManage(const UWorld* World);
    static FString GetCurrentGameNetDriverClassName();

private:
    static FNetDriverDefinition* FindGameNetDriverDefinition();

    static bool  bCaptured;
    static FName CapturedDriverClassName;
    static FName CapturedFallbackClassName;
};
```
Includes `Engine/Engine.h` for `FNetDriverDefinition`; forward-declares `ICSOnlineBackend`.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSNetDriverSwitcher.cpp`

**목적**: Implementation. Contains the mutation rule that must not be violated.

`FindGameNetDriverDefinition()`:
```cpp
if (!GEngine) return nullptr;
return GEngine->NetDriverDefinitions.FindByPredicate(
    [](const FNetDriverDefinition& D){ return D.DefName == NAME_GameNetDriver; });
```
(`NAME_GameNetDriver` is a hardcoded engine name, `UnrealNames.inl:222 REGISTER_NAME(282,GameNetDriver)`.)

**THE RULE, as a block comment at the top of `ApplyClassName`:**
```
// Mutate DriverClassName / DriverClassNameFallback IN PLACE only.
// NEVER Add/Remove/Insert/Empty on GEngine->NetDriverDefinitions:
// FNamedNetDriver holds a RAW FNetDriverDefinition* into that array
// (Engine.h:312, stored at UnrealEngine.cpp:15045), so any reallocation
// dangles the definition pointer of every live net driver.
```
If no `GameNetDriver` entry exists, log `LogCS, Error` and return false — **do not** Add one.

`IsSafeToSwitch(World)`: resolve `GEngine->GetWorldContextFromWorld(World)` and require `ActiveNetDrivers.Num() == 0`. Refuse with a specific error otherwise. In practice the host is in the title map (standalone, no drivers) at host time and the client likewise at join time, so this never fires in the happy path — it exists to catch a re-host without a proper teardown.

`ShouldManage(World)`:
```cpp
const UCSOnlineSettings& S = UCSOnlineSettings::Get();
if (!S.bManageNetDriverDefinitions) return false;
if (World && World->IsPlayInEditor() && !S.bManageNetDriverInPIE) return false;
return true;
```
The PIE guard is load-bearing. In the editor `GEngine` is `UEditorEngine`, which does not read `[/Script/Engine.GameEngine]`, so PIE's GameNetDriver comes from `[/Script/Engine.Engine]` and is plain `IpNetDriver` — correct for PIE. Forcing EOS or Steam drivers there would break PIE for no benefit (Steam is disabled in-editor regardless).

`ApplyForBackend` → `ShouldManage` → `IsSafeToSwitch` → `ApplyClassName(Backend.GetNetDriverClassName(), Settings.FallbackNetDriverClassName, World, OutError)`. Logs old→new at `LogCS, Log`; this line is the first thing to look for when a travel fails.

`RestoreDefaults()` writes the captured names back if `bCaptured`. **Called from `UCSOnlineSessionSubsystem::Deinitialize()`** — mandatory in-editor, because `GEngine` outlives a PIE session and an un-restored mutation silently leaks into the next run and into unrelated PIE tests.

Worth a comment: because `CreateNetDriver_Local` falls back to `DriverClassNameFallback` when the class fails to load *or* `CDO->IsAvailable()` is false (`UnrealEngine.cpp:15020-15024`), and both `USteamSocketsNetDriver::IsAvailable()` and `UNetDriverEIKBase::IsAvailable()` report honestly, a wrong or unusable setting degrades to `IpNetDriver` rather than crashing. Do not add your own availability probing on top.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineSessionSubsystem.h`

**목적**: The Blueprint-facing façade and the shared session flow. ZERO #if directives, zero backend name strings.

`UCLASS(BlueprintType) class CHRONOSPACE_API UCSOnlineSessionSubsystem : public UGameInstanceSubsystem`

**Lifecycle**
```cpp
virtual bool ShouldCreateSubsystem(UObject* Outer) const override;  // false if IsRunningCommandlet()
virtual void Initialize(FSubsystemCollectionBase& Collection) override;
virtual void Deinitialize() override;
```

**Backend (all `UFUNCTION`, Category="ChronoSpace|Online|Backend")**
```cpp
UFUNCTION(BlueprintPure)     ECSOnlineBackend GetActiveBackend() const;
UFUNCTION(BlueprintPure)     FText  GetActiveBackendDisplayName() const;
UFUNCTION(BlueprintPure)     ECSOnlineRole GetOnlineRole() const;
UFUNCTION(BlueprintCallable) void GetAvailableBackends(TArray<ECSOnlineBackend>& OutBackends) const;
UFUNCTION(BlueprintCallable) void GetCompiledBackends(TArray<ECSOnlineBackend>& OutBackends) const;
UFUNCTION(BlueprintPure)     bool CanSwitchBackend() const;
UFUNCTION(BlueprintCallable) bool SetActiveBackend(ECSOnlineBackend NewBackend, FString& OutError);
UFUNCTION(BlueprintCallable) void RequestBackendSwitchWithRestart(ECSOnlineBackend NewBackend);
```

**Identity** — `Login()`, `IsLoggedIn()`, `GetLocalPlayerNickname()`, `GetLocalPlayerNetIdString()`.

**Host / Find / Join / Leave**
```cpp
UFUNCTION(BlueprintCallable) void HostSession(const FCSHostSessionParams& Params);
UFUNCTION(BlueprintCallable) void FindSessions(const FCSFindSessionsParams& Params);
UFUNCTION(BlueprintCallable) void CancelFindSessions();
UFUNCTION(BlueprintCallable) void JoinSessionByResult(const FCSSessionSearchResult& Result);
UFUNCTION(BlueprintCallable) void JoinSessionByIndex(int32 ResultIndex);
UFUNCTION(BlueprintCallable) void LeaveSession();
UFUNCTION(BlueprintPure)     bool IsInSession() const;
UFUNCTION(BlueprintPure)     bool IsBusy() const;
UFUNCTION(BlueprintCallable) void GetLastSearchResults(TArray<FCSSessionSearchResult>& Out) const;
```

**Native-only entry points** (for the shim and for gameplay C++), NOT UFUNCTION:
```cpp
void JoinSessionNative(const FOnlineSessionSearchResult& SearchResult);
const TSharedPtr<ICSOnlineBackend>& GetBackend() const { return ActiveBackend; }
```
`JoinSessionNative` is what makes the currently-empty `UCSEIKSubsystem::JoinSessionForBlueprint` (`CSEIKSubsystem.cpp:220-222`) actually work.

**Delegates** — six `UPROPERTY(BlueprintAssignable, Category="ChronoSpace|Online")`: `OnLoginComplete`, `OnHostComplete`, `OnFindSessionsComplete`, `OnJoinSessionComplete`, `OnLeaveSessionComplete`, `OnBackendChanged`.

**Private state**
```cpp
enum class EOpState : uint8 { Idle, LoggingIn, Creating, Starting, Traveling, Finding, Joining, Destroying };

EOpState OpState = EOpState::Idle;
int32    OpToken = 0;            // bumped on BeginOp, FinishOp and Deinitialize
int32    SearchGeneration = 0;   // bumped on every FindSessions

TSharedPtr<ICSOnlineBackend> ActiveBackend;
ECSOnlineBackend ActiveBackendId = ECSOnlineBackend::None;
ECSOnlineRole    OnlineRole = ECSOnlineRole::Client;

// The OSS the currently-registered delegates belong to. Cleared delegates are
// cleared against THIS name, not against the (possibly new) active backend.
FName DelegateOwnerSubsystem = NAME_None;

TSharedPtr<FOnlineSessionSearch> CurrentSearch;
TArray<FCSSessionSearchResult>   CachedResults;

FCSHostSessionParams   PendingHostParams;
FCSFindSessionsParams  PendingFindParams;
FString                PendingTravelURL;
bool bPendingHostAfterLogin = false;
bool bPendingFindAfterLogin = false;

FTimerHandle OpTimeoutHandle;

FDelegateHandle LoginCompleteHandle, CreateSessionCompleteHandle, StartSessionCompleteHandle,
                FindSessionsCompleteHandle, JoinSessionCompleteHandle, DestroySessionCompleteHandle;
```

**Native callbacks** — `HandleLoginComplete(int32, bool, const FUniqueNetId&, const FString&)`, `HandleCreateSessionComplete(FName, bool)`, `HandleStartSessionComplete(FName, bool)`, `HandleFindSessionsComplete(bool)`, `HandleJoinSessionComplete(FName, EOnJoinSessionCompleteResult::Type)`, `HandleDestroySessionComplete(FName, bool)`.

**Private helpers** — `ResolveBackend`, `ClearAllDelegates`, `BeginOp`, `FinishOp`, `StartOpTimeout`, `ClearOpTimeout`, `HandleOpTimeout(int32 ExpectedToken)`, `BuildListenTravelURL`, `DoServerTravel`, `DoClientTravel`, `RebuildCachedResults`, `GetLocalPlayerControllerChecked`, `static ECSOnlineOpResult MapJoinResult(EOnJoinSessionCompleteResult::Type)`.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/Online/CSOnlineSessionSubsystem.cpp`

**목적**: The shared flow. Every correctness rule from the research lands here.

**Initialize** — `Super::Initialize(Collection)` FIRST (the existing bug at `CSEIKSubsystem.cpp:15-16` is a `return;` before `Super`). Then: resolve `OnlineRole` (`IsRunningDedicatedServer()` → DedicatedServer, else Client — promoted to ListenHost by `HostSession`); `FCSNetDriverSwitcher::CaptureDefaults()`; `ResolveBackend(Settings.ResolveStartupBackend(), Err)`; log the chosen backend, its subsystem FName, `CS_WITH_*` values and net-driver class in one line; if `bAutoLoginOnInitialize` call `Login()`.

**Deinitialize** — order matters and there is **no early return before `Super`** (the current class skips `Super::Deinitialize` on any failure, `CSEIKSubsystem.cpp:23-39`):
```
++OpToken;                 // invalidates every in-flight callback and timer
ClearOpTimeout();
ClearAllDelegates();
if (a session is registered) Session->DestroySession(NAME_GameSession);  // fire-and-forget
CurrentSearch.Reset(); CachedResults.Empty();
FCSNetDriverSwitcher::RestoreDefaults();
ActiveBackend.Reset();
Super::Deinitialize();
```
Do **not** call `Identity->Logout()`. Steam's always reports failure (`OnlineIdentityInterfaceSteam.cpp:68`) and EIK does not need it; the current call is the reason `Super::Deinitialize` gets skipped.

**ClearAllDelegates** resolves the session/identity interfaces via `IOnlineSubsystem::Get(DelegateOwnerSubsystem)` — not via `ActiveBackend` — so delegates registered before a backend switch are unregistered from the OSS that actually holds them. Clears all six handles, `Reset()`s each, then `DelegateOwnerSubsystem = NAME_None`. Every `Add*Delegate_Handle` call site sets `DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName()`.

**Re-entrancy / lifetime discipline** (the current class gets all of this wrong):
- `BeginOp(NewState, OutRejection)`: rejects `Busy` if `OpState != Idle`, `NoBackend` if `!ActiveBackend`; else sets state, `++OpToken`, `StartOpTimeout()`.
- Every completion handler, as its **first two statements**: null-check the subsystem and interface (`if (!Session.IsValid()) { FinishOp(Failed, ...); return; }`), then clear its own delegate handle. The current code dereferences `Get()` and `GetSessionInterface()` unchecked in four handlers — a guaranteed crash when Steam is not running, masked today only because EIK is always present.
- `StartOpTimeout()` uses `GetGameInstance()->GetTimerManager()` (GameInstance-owned, so it survives world changes) and captures the current `OpToken`; `HandleOpTimeout(ExpectedToken)` returns immediately if `ExpectedToken != OpToken`.
- `ClearOpTimeout()` is called before travel, and `OpState` is set to `Traveling` with no timer — a timeout firing across a `ServerTravel` would be worse than the hang it guards.
- `FinishOp` sets `Idle`, clears the timer, `++OpToken`. Broadcasting is left to callers because the delegate signatures differ.
- `LoginWithDeviceId`-style unbounded delegate accumulation (`CSEIKSubsystem.cpp:65-68` adds an `AddUObject` every call and never stores or clears a handle) is impossible here: `Login()` early-returns `Busy` if not `Idle`, and the handle is always stored and always cleared.

**HostSession(Params)**
1. `BeginOp(Creating, Rej)`; on rejection broadcast `OnHostComplete(Rej, ...)` and return.
2. `OnlineRole = ListenHost`.
3. Resolve `PendingHostParams`: default `MaxPlayers` / `TravelLevel` / `GameIdTag` from settings; strip any `?listen` from `ExtraTravelOptions` with a Warning.
4. If `ActiveBackend->NeedsExplicitLogin() && !IsLoggedIn()` → `bPendingHostAfterLogin = true; OpState = LoggingIn; Login();` and return. The host resumes from `HandleLoginComplete`.
5. Build `FOnlineSessionSettings Settings; ActiveBackend->FillCreateSettings(Settings, PendingHostParams, Config);`
6. Register `CreateSessionCompleteHandle`, set `DelegateOwnerSubsystem`, `Session->CreateSession(0, NAME_GameSession, Settings)`.

**HandleCreateSessionComplete** → clear handle; on failure `FinishOp(Failed)` + broadcast; on success `OpState = Starting`, register `StartSessionCompleteHandle`, `Session->StartSession(NAME_GameSession)`.

**HandleStartSessionComplete** → clear handle; on failure `FinishOp`. On success:
1. `PendingTravelURL = BuildListenTravelURL(PendingHostParams);`
2. `FCSNetDriverSwitcher::ApplyForBackend(*ActiveBackend, GetWorld(), Err)` — failure is logged as a Warning and travel proceeds (the engine's fallback will pick IpNetDriver), because aborting here would leave a live advertised session with no way in.
3. `ClearOpTimeout(); OpState = Traveling;`
4. Broadcast `OnHostComplete(Success, "")` **before** travelling, so UMG can close the menu while the world still exists.
5. `DoServerTravel()`.

Comment block on this handler: session-start and travel stay coupled and in this order per `Docs/DedicatedServer_Migration.md:708` (CreateSession must follow the first map load or the NetDriver is not picked up). Never insert a `DestroySession` here — `ACSGameMode::Logout` only preserves player slots while `bIsTearingDown` (`CSGameMode.cpp:138-145`), so a disconnect outside teardown makes every player lose their slot mid-travel and re-roll their character.

**BuildListenTravelURL** — resolve `TravelLevel` (or `Settings.DefaultHostLevel`) to a package path, append `ExtraTravelOptions`, append `?listen`, then `ActiveBackend->DecorateListenURL(URL)`. This is the only place `?listen` is ever written; `CSEIKSubsystem.cpp:164`, `CSLabyrinthKeyAltar.cpp:116` and `SCSServerTravelWidget.cpp:89` each hardcode their own today.

**DoServerTravel** — `UWorld* World = GetWorld(); if (World && World->GetAuthGameMode()) World->ServerTravel(PendingTravelURL, false);` then `FinishOp(Success, "")`.

**FindSessions(Params)**
1. `BeginOp(Finding, Rej)`.
2. `++SearchGeneration;` `CachedResults.Empty();`
3. Login gate as for host (`bPendingFindAfterLogin`).
4. `CurrentSearch = MakeShared<FOnlineSessionSearch>(); ActiveBackend->FillSearchSettings(*CurrentSearch, Params, Config);`
5. Register `FindSessionsCompleteHandle`, `Session->FindSessions(0, CurrentSearch.ToSharedRef())`.

**HandleFindSessionsComplete** → clear handle; `RebuildCachedResults()`; `FinishOp`; broadcast `OnFindSessionsComplete(Result, CachedResults, Error)`. **Never auto-joins** — `CSEIKSubsystem.cpp:206-212` joins the first result and `break`s, which makes a session list impossible.

**RebuildCachedResults** iterates `CurrentSearch->SearchResults`, skips `!Result.IsValid()` and `!ActiveBackend->ShouldAcceptSearchResult(...)`, and fills `FCSSessionSearchResult` with `ResultIndex = i`, `SearchGeneration`, `Backend = ActiveBackendId`, `PingInMs = Result.PingInMs`, `MaxPlayers = Session.SessionSettings.NumPublicConnections + NumPrivateConnections`, `OpenSlots = Session.NumOpenPublicConnections`, `CurrentPlayers = MaxPlayers - OpenSlots`, and the display strings read back off `SessionSettings` (`SessionNameKey`, `SETTING_MAPNAME`, `OwningUserName`). Log the accepted/rejected counts — "found 40, kept 0" versus "found 0" are completely different diagnoses.

**JoinSessionByResult** → validate `Result.SearchGeneration == SearchGeneration` (else `InvalidSession` with an explicit "search superseded" message; EIK's lobby cache is wiped by every new search, `OnlineSessionEOS.cpp:4526`) and `Result.Backend == ActiveBackendId`, then `JoinSessionByIndex(Result.ResultIndex)`.

**JoinSessionByIndex** → bounds-check against `CurrentSearch->SearchResults`, then `JoinSessionNative(...)`.

**JoinSessionNative(SearchResult)**
1. `BeginOp(Joining, Rej)`.
2. `FCSNetDriverSwitcher::ApplyForBackend(*ActiveBackend, GetWorld(), Err)` — **before** `JoinSession`, so the driver is right by the time the connect string is used.
3. Register `JoinSessionCompleteHandle`, `Session->JoinSession(0, NAME_GameSession, SearchResult)`.

**HandleJoinSessionComplete(SessionName, Result)** — corrected order relative to today's code, which travels first and checks the result afterwards (`CSEIKSubsystem.cpp:255-265`):
```cpp
clear handle;
if (Result != EOnJoinSessionCompleteResult::Success)
{ FinishOp(MapJoinResult(Result), ...); broadcast; return; }

FString ConnectInfo;
if (!Session->GetResolvedConnectString(SessionName, ConnectInfo) || ConnectInfo.IsEmpty())
{ FinishOp(Failed, TEXT("GetResolvedConnectString failed")); broadcast; return; }

ActiveBackend->DecorateClientTravelURL(ConnectInfo);
ClearOpTimeout(); OpState = Traveling;
broadcast OnJoinSessionComplete(Success, TEXT(""));
DoClientTravel(ConnectInfo);
```
`MapJoinResult` maps `SessionIsFull`→`SessionFull`, `SessionDoesNotExist`→`InvalidSession`, `AlreadyInSession`→`AlreadyInSession`, `CouldNotRetrieveAddress`/`UnknownError`→`Failed`.

Comment: `ConnectInfo` is `EOS:<ProductUserId>:<SocketName>:<Channel>` on EIK and `steam.<SteamID64>:<port>` on Steam. It is passed to `ClientTravel` verbatim and never parsed, logged-and-forgotten, or reconstructed. This is the single most backend-divergent value in the system; keeping it opaque is what makes one flow serve both.

**DoClientTravel** uses `GetGameInstance()->GetFirstLocalPlayerController(GetWorld())`, not `UGameplayStatics::GetPlayerController(World, 0)` (`CSEIKSubsystem.cpp:258`), then `PC->ClientTravel(ConnectString, TRAVEL_Absolute)` and `FinishOp(Success, "")`.

**LeaveSession** → `BeginOp(Destroying)`, register `DestroySessionCompleteHandle`, `Session->DestroySession(NAME_GameSession)`. This half of the lifecycle does not exist anywhere in the project today; `Docs/DedicatedServer_Migration.md:373` warns it leaves zombie sessions on the backend.

**SetActiveBackend(NewBackend, OutError)** → refuse unless `CanSwitchBackend()` (`OpState == Idle && !IsInSession() && Settings.bAllowRuntimeBackendSwitch`); `ClearAllDelegates()`; drop `CurrentSearch`/`CachedResults`; `++SearchGeneration`; swap `ActiveBackend`; `Settings.WritePreferredBackend(NewBackend)`; broadcast `OnBackendChanged(Old, New)`.

Comment on `SetActiveBackend`: this redirects only code routed through this subsystem. AdvancedSessions and `OnlineSubsystemUtils` Blueprint proxies resolve through `Online::GetSubsystem(World)` → `DefaultPlatformService` and will keep talking to EIK. Use `RequestBackendSwitchWithRestart` for a switch that moves those too.

**Hook for identity fallout**: `OnBackendChanged` should be bound (from `BP_CSGameInstance` or a small C++ hook) to `UCSPlayerSlotSubsystem::ResetAllSlots()` — `SlotByPlayerKey` is keyed on the UniqueNetId string (`CSPlayerSlotSubsystem.cpp:75`) and EOS ProductUserIds and SteamID64s stringify completely differently.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/CSEIKSubsystem.h`

**목적**: REWRITE in place. Becomes a thin deprecated forwarding shim so any existing or future Blueprint node bound to these four UFUNCTIONs keeps resolving.

Keep the class name `UCSEIKSubsystem`, the base `UGameInstanceSubsystem`, and the **exact signatures** of all five entry points. Drop every member variable — all six `FDelegateHandle`s, all four delegate objects, and `SessionSearch` — the shim is stateless.

```cpp
UCLASS(meta = (DeprecatedNode))
class CHRONOSPACE_API UCSEIKSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
              meta = (DeprecatedFunction,
                      DeprecationMessage = "Use CSOnlineSessionSubsystem::Login."))
    void LoginWithDeviceId();

    UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
              meta = (DeprecatedFunction,
                      DeprecationMessage = "Use CSOnlineSessionSubsystem::HostSession."))
    void CreateSession();

    UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
              meta = (DeprecatedFunction,
                      DeprecationMessage = "Use CSOnlineSessionSubsystem::FindSessions."))
    void FindSessions();

    UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
              meta = (DeprecatedFunction,
                      DeprecationMessage = "Use CSOnlineSessionSubsystem::JoinSessionByResult."))
    void JoinSessionForBlueprint(FBlueprintSessionResult& SearchResult);

    void JoinSession(const FOnlineSessionSearchResult& SearchResult);

private:
    UCSOnlineSessionSubsystem* GetOnline() const;
};
```
Keep `#include "FindSessionsCallbackProxy.h"` for `FBlueprintSessionResult` — it is `USTRUCT(BlueprintType)` at `FindSessionsCallbackProxy.h:11-17`, reachable because `OnlineSubsystemUtils` is already a public dependency (`ChronoSpace.Build.cs:27`).

Also replace the mojibake comment at `CSEIKSubsystem.h:67` (the file has a broken-encoding Korean comment) with plain ASCII: `// FOnlineSessionSearchResult is not Blueprint-exposable; native callers use this.`

RETENTION RATIONALE: verified zero references — `grep -rl --binary-files=text "CSEIK"` over `Content/` returns nothing, so today this shim protects nothing. It costs ~40 lines, keeps the four UFUNCTION signatures stable in case anything is wired later, and turns a class that is currently 100% dead into a documented redirect. See the Blueprint re-wiring note in `risks` for what actually has to change.

---

### `C:/Git/WindUp/Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp`

**목적**: REWRITE. Pure forwarding, ~60 lines. Fixes four defects by deletion.

```cpp
void UCSEIKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);   // the stray `return;` at old line 15 is GONE
}

void UCSEIKSubsystem::Deinitialize()
{
    Super::Deinitialize();           // no Logout, no early return
}

UCSOnlineSessionSubsystem* UCSEIKSubsystem::GetOnline() const
{
    UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UCSOnlineSessionSubsystem>() : nullptr;
}
```
Each forwarder logs `LogCS, Warning` once naming the replacement, then delegates:
- `LoginWithDeviceId()` → `Online->Login()`
- `CreateSession()` → `Online->HostSession(FCSHostSessionParams{})` (all defaults come from `UCSOnlineSettings`, so this now hosts an 8-player session on the configured map instead of a 2-player session on a hardcoded `L_StageSize`)
- `FindSessions()` → `Online->FindSessions(FCSFindSessionsParams{})`
- `JoinSessionForBlueprint(R)` → `Online->JoinSessionNative(R.OnlineResult)` — **this function had an empty body** (`CSEIKSubsystem.cpp:220-222`); it now works
- `JoinSession(R)` → `Online->JoinSessionNative(R)`

Every forwarder null-checks `GetOnline()` and logs an Error rather than dereferencing.

Defects removed by deletion rather than by patching: the pre-`Super` `return;` (:15); nine `IOnlineSubsystem::Get(TEXT("EIK"))` literals (:23, 44, 87, 122, 147, 170, 200, 226, 251); the per-call unremoved `OnLoginCompleteDelegates->AddUObject` (:65-68); four unchecked `GetSessionInterface()` dereferences in completion handlers; the travel-before-checking-the-result inversion (:255-265); the auto-join-first-result loop (:206-212); the hardcoded `NumPublicConnections = 2` (:110) and `/Game/02_Map/L_StageSize?listen` (:115, :164); and the per-launch `FGuid::NewGuid()` device id (:58).

---

### `C:/Git/WindUp/Source/ChronoSpace/Debug/CSOnlineDebugCommands.cpp`

**목적**: Console commands for flipping backends and driving the flow without UI. Non-shipping only. Lives in Debug/ per folder convention.

Whole file inside `#if !UE_BUILD_SHIPPING`. No header — `FAutoConsoleCommand` statics only, plus a file-local helper resolving the subsystem from `GEngine->GetWorldContexts()`' first `GameInstance`.

Commands (`FAutoConsoleCommandWithWorldAndArgs` so the world is supplied):
- `cs.Online.Status` — active backend, subsystem FName, `IsAvailable`/`GetUnavailableReason` for every compiled backend, login status, local net id, `UsesLobbies()`, current `GameNetDriver` `DriverClassName` via `FCSNetDriverSwitcher::GetCurrentGameNetDriverClassName()`, `CS_WITH_EIK`/`CS_WITH_STEAM`/`CS_WITH_STEAMSOCKETS`, `OpState`. This one command answers most "why can nobody see my session" questions.
- `cs.Online.Backend <EIK|STEAM|NULL>` — `SetActiveBackend`, printing `OutError` on refusal.
- `cs.Online.Login`
- `cs.Online.Host [MaxPlayers]`
- `cs.Online.Find`
- `cs.Online.List` — prints `CachedResults` with index, name, ping, slots, so `cs.Online.Join <index>` is usable.
- `cs.Online.Join <index>`
- `cs.Online.Leave`

All output via `UE_LOG(LogCS, Display, ...)` so it lands in the same category as everything else and is reachable through the MCP `LogsToolset`.

Why this file is worth writing: Steam cannot be exercised in PIE at all (`OnlineSubsystemSteam.cpp:479-484`), so every Steam test is a Standalone or packaged process with no editor UI attached. Console commands are the only practical driver until the UMG is migrated.

---

## 7. 리스크

- The biggest risk is not in the C++ at all: this design changes nothing at runtime until Blueprints are re-wired. UCSEIKSubsystem is referenced by ZERO assets (grep -rl "CSEIK" over Content/ returns nothing), so the entire new layer is dead code on merge. The live session path is three incompatible Blueprint stacks: (1) EIK-native lobby nodes in WBP_Title and WBP_LobbyCreateAndJoin (CreateEIKLobby / FindEIKSessions / JoinEIKSessions / DestroyEIKSessions / EEIKJoinResult / EIKAttribute) which have NO Steam equivalent and must be REWRITTEN against the new facade, not reconfigured; (2) AdvancedSessions proxies in BP_SessionUI, BP_CSGameInstance and WBP_EOSLobby which survive a Steam switch but always resolve through DefaultPlatformService, so they will keep talking to EIK while the facade talks to Steam; (3) the dead C++ path. Budget the UMG migration as the real work - the C++ here is the smaller half.
- Runtime backend switching is genuinely partial and will produce confusing split-brain behaviour if that is not understood. SetActiveBackend redirects only code routed through UCSOnlineSessionSubsystem. AdvancedFriendsGameInstance's invite flow, CreateSessionCallbackProxyAdvanced, FindSessionsCallbackProxyAdvanced and JoinSessionCallbackProxy all call Online::GetSubsystem(World) -> DefaultPlatformService. Until the BP graphs move onto the facade, treat RequestBackendSwitchWithRestart (or the -ini: command line) as the only trustworthy switch.
- SteamSockets forces Steam OSS creation at process start. FSteamSocketsModule::StartupModule calls IOnlineSubsystem::Get(STEAM_SUBSYSTEM) at load (SteamSocketsModule.cpp:14) and latches bEnabled ONCE with no retry. So in any packaged/-game build with the plugin enabled, SteamAPI_InitEx runs at boot even when the player chose EIK - the Steam overlay initialises, and if the Steam client starts AFTER your game, SteamSockets stays disabled for the whole process with no way to recover short of a relaunch.
- NetDriverDefinitions mutation is safe only in place. FNamedNetDriver holds a raw FNetDriverDefinition* into GEngine->NetDriverDefinitions (Engine.h:312, stored at UnrealEngine.cpp:15045). Any future 'improvement' that Adds a second GameNetDriver entry, or Empties and rebuilds the array, will dangle the definition pointer of every live net driver. The rule is written as a comment in CSNetDriverSwitcher.cpp; it needs to survive code review pressure.
- GEngine outlives PIE in the editor, so an un-restored NetDriverDefinitions mutation leaks into the next PIE session and into unrelated tests. RestoreDefaults() in Deinitialize is not optional cleanliness - it is the thing that stops a Steam-backend test from silently breaking the next person's PIE run.
- UCSPlayerSlotSubsystem keys players by the UniqueNetId STRING (net:<id>|<lpid>, CSPlayerSlotSubsystem.cpp:75). EOS ProductUserIds and SteamID64s stringify completely differently, so SlotByPlayerKey is garbage across a backend switch. Worse, the fallback key is remote:<controllerId> (:91-92) which CANNOT distinguish two remote clients - if the NetId path ever returns empty on Steam during PostLogin, every remote player collapses into one key and both spawn as the same character. Binding OnBackendChanged to ResetAllSlots is a mitigation, not a fix.
- ECSPlayerSlot has exactly two values (Player0/Player1, CSPlayerState.h:12) and PickLowestFreeSlot returns Player1 when both are taken (CSPlayerSlotSubsystem.cpp:107). The stated 2-8 player target is not representable. Setting DefaultMaxPlayers=8 in the session layer will let 8 players connect into a 2-slot character model. Either cap MaxPlayers at 2 until the slot model is widened, or widen it in the same milestone.
- CSSplitScreenSubsystem::ResolveRemoteCharacter picks the FIRST non-locally-controlled ACSCharacterPlayer (CSSplitScreenSubsystem.cpp:130-145). With more than 2 players that is arbitrary. Its cached TWeakObjectPtrs also go stale across non-seamless travel and nothing resets them - the first frames after every travel push a stale or empty secondary view. Raising the player cap makes both of these visible.
- bAutomaticallySetupEIK=True (currently set) makes the plugin rewrite Config/DefaultEngine.ini as RAW TEXT at every module startup, including force-replacing DefaultPlatformService= with EIK (EOSIntegrationKit.cpp:42-150). If anyone re-enables it - or toggles the checkbox in Project Settings, which runs a near-duplicate from PostEditChangeProperty - every Steam edit silently reverts and the failure looks like 'Steam config keeps not working'.
- The credential string "noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN" is compared with plain FString == at UserManagerEOS.cpp:1077 to decide whether to auto-create a Device ID on first launch. Any reconstruction from an enum (which is what EIK's own Blueprint node does, producing the fully-qualified form at EIK_Login_AsyncFunction.cpp:72) makes first launch on a fresh machine return EOS_NotFound and fail. It must stay a hardcoded literal, and a well-meaning refactor to 'derive it from EEIK_EExternalCredentialType' will break new-machine login only - which is the hardest kind of bug to notice.
- EIK_Login_AsyncFunction.cpp has been LOCALLY MODIFIED in this repo (it carries a GetFallbackDeviceLoginId() helper reading [/Script/ChronoSpace.Login] PersistentDeviceLoginGuid, added in 0181dd32 / 637330d7). Any EIK plugin upgrade clobbers it. Since the new C++ path uses UCSLoginIdLibrary directly, the local patch becomes redundant once the Blueprints move onto the facade - retiring it should be an explicit step, not something discovered after an upgrade.
- Config/DefaultEngine.ini commits a live EOS ClientId, ClientSecret and EncryptionKey in plaintext (~line 232). Unrelated to this work but sitting in the block being edited, and this change makes the repo more likely to be shared or built publicly. Rotate before that happens.
- Nothing in the codebase currently calls EndSession or DestroySession from C++, so the backend accumulates zombie sessions (flagged at Docs/DedicatedServer_Migration.md:373). LeaveSession adds the missing half, but Deinitialize can only fire DestroySession and forget - at GameInstance teardown there is no way to await the callback. Some sessions will still leak on hard exits.
- The EIK lobby-vs-sessions decision (bEIKUseLobbies) is a one-way-ish door mid-project: create and find must agree, and JoinSession routes on the flag carried BY THE SEARCH RESULT (OnlineSessionEOS.cpp:2661). Flipping it while players are on mixed builds means a client on the old setting silently finds nothing, or joins through the wrong EOS API. Flip it between milestones, never as a hotfix.
- There is no Server target (only ChronoSpace.Target.cs and ChronoSpaceEditor.Target.cs), so every #if UE_SERVER / WITH_SERVER_CODE path and the BuildDedicatedServerCredentials seam are untested by the current build. The seam exists to avoid painting into a corner, not because it works.
- Enabling three plugins changes the compiled game module, so Binaries/Win64/UnrealEditor-ChronoSpace.dll must be rebuilt and committed with this change (per .claude/rules/02-generated-output.md, that DLL is deliberately tracked). If it is not, teammates' editors load a DLL whose CS_WITH_* values disagree with the .uproject, and the Steam adapter appears to not exist for reasons that make no sense from the source.

## 8. 미결 사항

- EIK Sessions or EIK Lobbies? bEIKUseLobbies=False preserves today's behaviour exactly, but EOS Sessions give you NO host migration and NO RTC voice room, and the Sessions search injects NumPublicConnections>=1 plus a BucketId equality filter (so two builds with different BuildUniqueId can never see each other). Lobbies give migration and voice but apply no bucket filter at all, so lobbies leak across builds unless you filter on BuildUniqueId yourself. For 2-8 player drop-in co-op, lobbies are probably the right long-term answer - but it is a behaviour change that should be its own milestone, and it makes EIK and Steam structurally more similar which may or may not be desirable while you plan to drop EIK.
- Should the player-slot key be namespaced by backend (backend:net:<id>|<lpid>) instead of just reset on switch? Namespacing is strictly better - it survives switching back and forth, which a reset does not - but it touches UCSPlayerSlotSubsystem, whose comments document a real UE 5.5 + EIK linker workaround at CSPlayerSlotSubsystem.cpp:60-64 that must not be 'simplified'. I have specified the ResetAllSlots hook because it is the smaller blast radius; namespacing is the better fix if you are willing to touch that file.
- When does ECSPlayerSlot grow past 2? The session layer will happily advertise 8 slots while the character model supports 2. Either DefaultMaxPlayers stays at 2 until the slot model is widened, or the two changes land together. Shipping the 8-slot session first will produce 'both players are the same character' reports that look like online bugs and are not.
- Real Steam appid timeline. Everything about 480 is workable but 480 shapes design decisions (mandatory GameIdTag filtering, no stats, wrong presence strings). If a real appid lands within a month it may be worth waiting rather than building 480 workarounds into UI copy.
- Do the EIK-native Blueprint graphs (WBP_Title, WBP_LobbyCreateAndJoin) get rewritten onto the facade now, or does a parallel path run for a while? They use CreateEIKLobby / FindEIKSessions / JoinEIKSessions, which are EOS Lobby APIs with no Steam equivalent, so 'Steam works' is not true in any user-visible sense until they move. Running both in parallel means two session stacks can create sessions simultaneously, which is worse than either alone.
- Should BP_CSGameInstance be reparented to a new C++ UCSGameInstance : UAdvancedFriendsGameInstance? There is no UGameInstance subclass in Source/ at all today, so there is no C++ seam for Init()/OnStart() hooks, invite-accepted routing, or binding OnBackendChanged. Not required by this design (everything lives on GameInstanceSubsystems), but the invite flow in particular currently resolves through DefaultPlatformService with no way to intercept it.
- UE_PROJECT_STEAMGAMEDIR value. It only matters on the server-browser path, which we do not use, but it defaults to "unrealtest" and setting it wrong later is easy to forget. Worth choosing the string now even though nothing reads it yet.
- Is the ~1 year single-process dual-backend actually needed, or would per-build selection be enough? Compile-time-only selection (one ini fragment per build config, no runtime switcher, no NetDriverDefinitions mutation) removes the entire class of risks around split-brain BP proxies and net driver mutation. The runtime switcher exists because the requirement asked for it; if in practice QA always relaunches anyway, the restart-based path is strictly safer and the in-session switch could be dropped.
- How should a mid-session backend failure be surfaced? Right now a Steam client that quits mid-game produces a normal network disconnect and the facade has no opinion. Whether to attempt a fallback-to-EIK reconnect, or just report the disconnect, is a product decision that affects whether the search-generation/session-token model needs to survive a backend change mid-flight.
