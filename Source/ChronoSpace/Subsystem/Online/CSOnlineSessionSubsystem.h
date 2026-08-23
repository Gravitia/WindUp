// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystem/Online/CSOnlineBackend.h"
#include "Subsystem/Online/CSOnlineTypes.h"
#include "CSOnlineSessionSubsystem.generated.h"

class APlayerController;

/**
 * The Blueprint-facing facade and the shared session flow for the whole online layer.
 *
 * THIS FILE CONTAINS ZERO #if DIRECTIVES AND ZERO BACKEND NAME STRINGS, DELIBERATELY.
 * Everything backend-specific lives behind ICSOnlineBackend. If a #if CS_WITH_* ever
 * appears in this class, the abstraction has failed and the year-end Steam-only cutover
 * stops being a file deletion.
 *
 * Shared flow, and the ordering it must not break:
 *   Host:   Login -> FillCreateSettings -> CreateSession -> StartSession
 *           -> SwitchNetDriver -> ServerTravel(URL?listen)
 *   Client: FindSessions -> (BP picks an index) -> SwitchNetDriver -> JoinSession
 *           -> check Result == Success -> GetResolvedConnectString -> ClientTravel
 *
 * Load-bearing constraints inherited from Docs/DedicatedServer_Migration.md and the slot
 * subsystem:
 *  - Session-start and travel stay coupled and in that order (:708). ServerTravel fires
 *    from HandleStartSessionComplete and is never decoupled from it.
 *  - Never DestroySession before travel. ACSGameMode::Logout releases the player slot only
 *    while !GetWorld()->bIsTearingDown (CSGameMode.cpp:138-145), so a disconnect outside a
 *    tearing-down world makes every player lose their slot mid-travel and re-roll their
 *    character.
 *  - No new async step before PostLogin. The slot is stamped onto PlayerState before
 *    Super::PostLogin because RestartPlayer -> GetDefaultPawnClassForController needs it at
 *    that instant (CSGameMode.cpp:52-53, :160-161, the "EOS reconnect race").
 *  - The travel URL is built by this layer, never typed at a call site. "?listen" appears
 *    exactly once in the codebase, in BuildListenTravelURL.
 */
UCLASS(BlueprintType)
class CHRONOSPACE_API UCSOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- Lifecycle ----------------------------------------------------------------

	/** False for commandlets: nothing headless should be creating online sessions. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Calls Super FIRST -- the existing bug at CSEIKSubsystem.cpp:15-16 is a `return;`
	 * placed before Super::Initialize. Then resolves the role, captures the net-driver
	 * defaults, resolves the backend, and logs the choice with the CS_WITH_* values.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Invalidates in-flight callbacks, clears every delegate handle, restores the net
	 * driver definition and calls Super unconditionally.
	 *
	 * There is deliberately NO early return before Super::Deinitialize here: the current
	 * class skips it on any failure (CSEIKSubsystem.cpp:23-39) because it tries to Logout
	 * first. Do not add a Logout call back -- Steam's always reports failure
	 * (OnlineIdentityInterfaceSteam.cpp:68) and EIK does not need one.
	 */
	virtual void Deinitialize() override;

	// --- Backend ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Backend")
	ECSOnlineBackend GetActiveBackend() const;

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Backend")
	FText GetActiveBackendDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Backend")
	ECSOnlineRole GetOnlineRole() const;

	/** Backends compiled into this build whose IsAvailable() is true right now. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Backend")
	void GetAvailableBackends(TArray<ECSOnlineBackend>& OutBackends) const;

	/** Backends compiled into this build at all, available or not, so UI can grey out the rest. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Backend")
	void GetCompiledBackends(TArray<ECSOnlineBackend>& OutBackends) const;

	/** OpState == Idle && !IsInSession() && UCSOnlineSettings::bAllowRuntimeBackendSwitch. */
	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Backend")
	bool CanSwitchBackend() const;

	/**
	 * Swaps the adapter in-process and broadcasts OnBackendChanged.
	 *
	 * HARD LIMIT, AND IT IS NOT A SMALL ONE: this redirects only code routed through this
	 * subsystem. Every AdvancedSessions / OnlineSubsystemUtils Blueprint node
	 * (CreateSessionCallbackProxy, FindSessionsCallbackProxyAdvanced, JoinSessionCallbackProxy,
	 * AdvancedFriendsGameInstance's invite flow) resolves through Online::GetSubsystem(World)
	 * -> DefaultPlatformService and keeps talking to the old backend. Since BP_CSGameInstance,
	 * BP_SessionUI and WBP_EOSLobby are built on exactly those nodes, a runtime switch leaves
	 * them on one backend while this facade is on another. Use RequestBackendSwitchWithRestart
	 * for a switch that moves those too.
	 *
	 * Do NOT reach for IOnlineSubsystem::ReloadDefaultSubsystem() instead: its own header
	 * says editor-only and crash-prone (OnlineSubsystemModule.h:193-197).
	 */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Backend")
	bool SetActiveBackend(ECSOnlineBackend NewBackend, FString& OutError);

	/**
	 * Writes DefaultPlatformService into GEngineIni and PreferredBackend into
	 * GGameUserSettingsIni, then asks the player to relaunch. This is the only switch that
	 * also moves the legacy AdvancedSessions Blueprint graphs.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Backend")
	void RequestBackendSwitchWithRestart(ECSOnlineBackend NewBackend);

	// --- Identity -----------------------------------------------------------------

	/** No-ops to success when the backend does not need an explicit login or is already logged in. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Identity")
	void Login();

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Identity")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Identity")
	FString GetLocalPlayerNickname() const;

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Identity")
	FString GetLocalPlayerNetIdString() const;

	// --- Host / Find / Join / Leave -----------------------------------------------

	/** Logs in first when the backend requires it, then creates, starts and travels. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void HostSession(const FCSHostSessionParams& Params);

	/** Bumps SearchGeneration. NEVER auto-joins a result -- that is the caller's decision. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void FindSessions(const FCSFindSessionsParams& Params);

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void CancelFindSessions();

	/**
	 * Validates Result.SearchGeneration against the live counter before joining.
	 *
	 * A stale generation returns ECSOnlineOpResult::InvalidSession with an explicit
	 * "search superseded" message: EIK's JoinLobbySession requires the lobby to still be
	 * in LobbySearchResultsCache keyed by session-id string (OnlineSessionEOS.cpp:4068-4076),
	 * and that cache is wiped at the start of every StartLobbySearch (:4526). Failing here
	 * with a clear line beats failing deep inside EOS.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void JoinSessionByResult(const FCSSessionSearchResult& Result);

	/** Bounds-checked index into the live FOnlineSessionSearch. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void JoinSessionByIndex(int32 ResultIndex);

	/** The other half of the session lifecycle, which does not exist anywhere in the project today. */
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Session")
	bool IsInSession() const;

	UFUNCTION(BlueprintPure, Category = "ChronoSpace|Online|Session")
	bool IsBusy() const;

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Session")
	void GetLastSearchResults(TArray<FCSSessionSearchResult>& Out) const;

	// --- Native-only entry points (deliberately NOT UFUNCTION) ---------------------

	/**
	 * Joins a raw OSS search result. This is what makes the currently-empty
	 * UCSEIKSubsystem::JoinSessionForBlueprint (CSEIKSubsystem.cpp:220-222) actually work,
	 * and it is the entry point gameplay C++ should use.
	 */
	void JoinSessionNative(const FOnlineSessionSearchResult& SearchResult);

	/** Never null once Initialize has run: the registry always terminates on the Null adapter. */
	const TSharedPtr<ICSOnlineBackend>& GetBackend() const { return ActiveBackend; }

	// --- Blueprint-assignable completion delegates ---------------------------------

	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnLoginCompleteBP OnLoginComplete;

	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnHostCompleteBP OnHostComplete;

	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnFindSessionsCompleteBP OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnJoinSessionCompleteBP OnJoinSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnLeaveSessionCompleteBP OnLeaveSessionComplete;

	/**
	 * Bind this to UCSPlayerSlotSubsystem::ResetAllSlots(). SlotByPlayerKey is keyed on the
	 * UniqueNetId string (CSPlayerSlotSubsystem.cpp:75) and EOS ProductUserIds and
	 * SteamID64s stringify completely differently, so the map is garbage across a switch.
	 */
	UPROPERTY(BlueprintAssignable, Category = "ChronoSpace|Online")
	FCSOnBackendChangedBP OnBackendChanged;

private:
	/** Single in-flight operation at a time. Guarded by BeginOp / FinishOp. */
	enum class EOpState : uint8
	{
		Idle,
		LoggingIn,
		Creating,
		Starting,
		Traveling,
		Finding,
		Joining,
		Destroying
	};

	// --- Native OSS callbacks ------------------------------------------------------
	// Every one of these does the same two things first: null-check the subsystem and its
	// interface, then clear its own delegate handle. The current code dereferences
	// IOnlineSubsystem::Get() and GetSessionInterface() unchecked in four handlers
	// (CSEIKSubsystem.cpp:122-124, :147-149, :200-202, :251-253) -- a guaranteed crash when
	// the backend is not running, masked today only because EIK is always present.

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// --- Helpers -------------------------------------------------------------------

	/**
	 * Takes the requested backend as an intent and walks
	 * UCSOnlineSettings::BackendFallbackOrder for something actually available, logging a
	 * Warning that names BOTH the requested and the actual backend. Keeping intent
	 * (UCSOnlineSettings::ResolveStartupBackend) separate from availability is what lets
	 * the log say "requested Steam, Steam unavailable (client not running), fell back to
	 * EIK" instead of just reporting EIK.
	 */
	bool ResolveBackend(ECSOnlineBackend Requested, FString& OutError);

	/**
	 * Resolves the session/identity interfaces via IOnlineSubsystem::Get(DelegateOwnerSubsystem),
	 * NOT via ActiveBackend, so delegates registered before a backend switch are
	 * unregistered from the OSS that actually holds them. Clears and Resets all six
	 * handles, then sets DelegateOwnerSubsystem = NAME_None.
	 */
	void ClearAllDelegates();

	/** Rejects Busy if OpState != Idle and NoBackend if !ActiveBackend; otherwise sets the state, bumps OpToken and starts the timeout. */
	bool BeginOp(EOpState NewState, ECSOnlineOpResult& OutRejection);

	/** Sets Idle, clears the timeout, bumps OpToken. Broadcasting is left to the caller because the delegate signatures differ. */
	void FinishOp(ECSOnlineOpResult Result, const FString& ErrorMessage);

	/** Uses GetGameInstance()->GetTimerManager() -- GameInstance-owned, so the timer survives a world change -- and captures the current OpToken. */
	void StartOpTimeout();

	/**
	 * Always called before travel. OpState becomes Traveling with no timer running: a
	 * timeout firing across a ServerTravel would be worse than the hang it guards against.
	 */
	void ClearOpTimeout();

	/** Returns immediately when ExpectedToken != OpToken, which is how a superseded operation's timer becomes harmless. */
	void HandleOpTimeout(int32 ExpectedToken);

	/**
	 * The ONLY place "?listen" is ever written. Resolves TravelLevel (or
	 * UCSOnlineSettings::DefaultHostLevel), appends ExtraTravelOptions, appends "?listen",
	 * then lets the backend decorate it. CSEIKSubsystem.cpp:164, CSLabyrinthKeyAltar.cpp:116
	 * and SCSServerTravelWidget.cpp:89 each hardcode their own today; they should not.
	 */
	FString BuildListenTravelURL(const FCSHostSessionParams& Params) const;

	void DoServerTravel();

	/**
	 * ConnectString is EOS:<ProductUserId>:<SocketName>:<Channel> on EIK and
	 * steam.<SteamID64>:<port> on Steam. It is the single most backend-divergent value in
	 * the system, it is produced by exactly one GetResolvedConnectString call, and it is
	 * passed to ClientTravel VERBATIM -- never parsed, split or reconstructed. Keeping it
	 * opaque is what lets one flow serve both backends.
	 */
	void DoClientTravel(const FString& ConnectString);

	/** Filters CurrentSearch->SearchResults through ShouldAcceptSearchResult and logs the accepted/rejected counts -- "found 40, kept 0" and "found 0" are completely different diagnoses. */
	void RebuildCachedResults();

	/** GetGameInstance()->GetFirstLocalPlayerController(GetWorld()), not UGameplayStatics::GetPlayerController(World, 0) (CSEIKSubsystem.cpp:258). */
	APlayerController* GetLocalPlayerControllerChecked() const;

	static ECSOnlineOpResult MapJoinResult(EOnJoinSessionCompleteResult::Type Result);

	// --- State ---------------------------------------------------------------------

	EOpState OpState = EOpState::Idle;

	/** Bumped on BeginOp, FinishOp and Deinitialize; invalidates every in-flight callback and timer. */
	int32 OpToken = 0;

	/** Bumped on every FindSessions. Compared against FCSSessionSearchResult::SearchGeneration on join. */
	int32 SearchGeneration = 0;

	TSharedPtr<ICSOnlineBackend> ActiveBackend;
	ECSOnlineBackend ActiveBackendId = ECSOnlineBackend::None;
	ECSOnlineRole    OnlineRole = ECSOnlineRole::Client;

	/**
	 * The OSS the currently-registered delegates belong to. Delegates are cleared against
	 * THIS name, not against the (possibly new) active backend -- otherwise a backend
	 * switch leaves live callbacks registered on the old subsystem forever.
	 * Every Add*Delegate_Handle call site sets this to ActiveBackend->GetSubsystemName().
	 */
	FName DelegateOwnerSubsystem = NAME_None;

	TSharedPtr<FOnlineSessionSearch> CurrentSearch;
	TArray<FCSSessionSearchResult>   CachedResults;

	FCSHostSessionParams  PendingHostParams;
	FCSFindSessionsParams PendingFindParams;
	FString               PendingTravelURL;
	bool bPendingHostAfterLogin = false;
	bool bPendingFindAfterLogin = false;

	FTimerHandle OpTimeoutHandle;

	// Six handles, one per OSS delegate. Each is stored on registration, cleared in its own
	// completion handler, and cleared again in ClearAllDelegates() from Deinitialize and
	// from SetActiveBackend. This is what makes the unbounded delegate accumulation at
	// CSEIKSubsystem.cpp:65-68 -- an AddUObject on every call with no handle ever stored --
	// structurally impossible here.
	FDelegateHandle LoginCompleteHandle;
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle StartSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
