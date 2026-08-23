// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/Online/CSOnlineSessionSubsystem.h"

#include "ChronoSpace.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "Settings/CSOnlineSettings.h"
#include "Subsystem/Online/CSNetDriverSwitcher.h"
#include "Subsystem/Online/CSOnlineBackendRegistry.h"
#include "TimerManager.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * Resolves the session interface on an EXPLICITLY NAMED subsystem, null-checking each step.
	 *
	 * IOnlineSubsystem::Get() is never called without an argument anywhere in this layer: the
	 * whole design depends on addressing backends by name so that DefaultPlatformService is
	 * irrelevant to us. Returning an empty pointer instead of dereferencing is the specific fix
	 * for the unconditional dereferences at CSEIKSubsystem.cpp:122-124, :147-149, :200-202 and
	 * :251-253, which crash whenever the backend is not actually running.
	 */
	IOnlineSessionPtr ResolveSessionInterface(FName SubsystemName)
	{
		if (SubsystemName.IsNone())
		{
			return IOnlineSessionPtr();
		}

		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(SubsystemName);
		return Subsystem ? Subsystem->GetSessionInterface() : IOnlineSessionPtr();
	}

	/** Identity counterpart of ResolveSessionInterface, with the same null-checking discipline. */
	IOnlineIdentityPtr ResolveIdentityInterface(FName SubsystemName)
	{
		if (SubsystemName.IsNone())
		{
			return IOnlineIdentityPtr();
		}

		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(SubsystemName);
		return Subsystem ? Subsystem->GetIdentityInterface() : IOnlineIdentityPtr();
	}

	const TCHAR* OpResultToString(ECSOnlineOpResult Result)
	{
		switch (Result)
		{
		case ECSOnlineOpResult::Success:          return TEXT("Success");
		case ECSOnlineOpResult::Failed:           return TEXT("Failed");
		case ECSOnlineOpResult::Busy:             return TEXT("Busy");
		case ECSOnlineOpResult::NoBackend:        return TEXT("NoBackend");
		case ECSOnlineOpResult::NotLoggedIn:      return TEXT("NotLoggedIn");
		case ECSOnlineOpResult::InvalidSession:   return TEXT("InvalidSession");
		case ECSOnlineOpResult::AlreadyInSession: return TEXT("AlreadyInSession");
		case ECSOnlineOpResult::SessionFull:      return TEXT("SessionFull");
		case ECSOnlineOpResult::VersionMismatch:  return TEXT("VersionMismatch");
		case ECSOnlineOpResult::Cancelled:        return TEXT("Cancelled");
		case ECSOnlineOpResult::Timeout:          return TEXT("Timeout");
		default:                                  return TEXT("Unknown");
		}
	}

	/**
	 * Local, rather than the engine's global LexToString(EOnJoinSessionCompleteResult::Type)
	 * (OnlineSessionInterface.h): that name has dozens of global overloads in Core and the enum
	 * is unscoped, so resolving it is a needless overload-resolution gamble in a log statement.
	 */
	const TCHAR* JoinResultToString(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:                 return TEXT("Success");
		case EOnJoinSessionCompleteResult::SessionIsFull:           return TEXT("SessionIsFull");
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:     return TEXT("SessionDoesNotExist");
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: return TEXT("CouldNotRetrieveAddress");
		case EOnJoinSessionCompleteResult::AlreadyInSession:        return TEXT("AlreadyInSession");
		case EOnJoinSessionCompleteResult::UnknownError:            return TEXT("UnknownError");
		default:                                                    return TEXT("Unknown");
		}
	}

	const TCHAR* OnlineRoleToString(ECSOnlineRole Role)
	{
		switch (Role)
		{
		case ECSOnlineRole::Client:           return TEXT("Client");
		case ECSOnlineRole::ListenHost:       return TEXT("ListenHost");
		case ECSOnlineRole::DedicatedServer:  return TEXT("DedicatedServer");
		default:                              return TEXT("Unknown");
		}
	}

	FString MakeRejectionMessage(ECSOnlineOpResult Rejection, const TCHAR* Operation)
	{
		switch (Rejection)
		{
		case ECSOnlineOpResult::Busy:
			return FString::Printf(TEXT("%s rejected: another online operation is already in flight."), Operation);
		case ECSOnlineOpResult::NoBackend:
			return FString::Printf(TEXT("%s rejected: no online backend is active."), Operation);
		default:
			return FString::Printf(TEXT("%s rejected: %s."), Operation, OpResultToString(Rejection));
		}
	}
}

// ---------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------

bool UCSOnlineSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Nothing headless should be opening network sessions: a cook or a resave commandlet that
	// touched the online stack would sit on a login timeout for no reason.
	return !IsRunningCommandlet();
}

void UCSOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Super FIRST. The existing bug at CSEIKSubsystem.cpp:15-16 is a bare `return;` placed
	// before Super::Initialize, which leaves the subsystem half-constructed.
	Super::Initialize(Collection);

	const UCSOnlineSettings& Config = UCSOnlineSettings::Get();

	// Client is the default; HostSession promotes to ListenHost.
	OnlineRole = IsRunningDedicatedServer() ? ECSOnlineRole::DedicatedServer : ECSOnlineRole::Client;

	// Snapshot the shipped GameNetDriver entry before anything can retarget it. Idempotent, and
	// it must happen before the first ApplyForBackend or RestoreDefaults would restore our own
	// mutation as if it were the default.
	FCSNetDriverSwitcher::CaptureDefaults();

	// ResolveStartupBackend() is INTENT only -- it deliberately does not consult IsAvailable().
	// ResolveBackend turns that intent into an actually-available adapter and logs both names
	// when they differ.
	FString ResolveError;
	ResolveBackend(Config.ResolveStartupBackend(), ResolveError);

	TArray<ECSOnlineBackend> CompiledBackends;
	FCSOnlineBackendRegistry::Get().GetCompiledBackends(CompiledBackends);

	FString CompiledList;
	for (const ECSOnlineBackend CompiledId : CompiledBackends)
	{
		if (!CompiledList.IsEmpty())
		{
			CompiledList += TEXT(", ");
		}
		CompiledList += UCSOnlineSettings::LexToString(CompiledId);
	}

	// One line carrying everything a bug report needs: which backend won, which OSS module that
	// actually is, what this build was compiled with, and which transport travel will use.
	UE_LOG(LogCS, Log,
		TEXT("[Online] Initialized. Backend=%s (subsystem '%s'), role=%s, compiled=[%s], GameNetDriver='%s'."),
		UCSOnlineSettings::LexToString(ActiveBackendId),
		ActiveBackend.IsValid() ? *ActiveBackend->GetSubsystemName().ToString() : TEXT("<none>"),
		OnlineRoleToString(OnlineRole),
		*CompiledList,
		*FCSNetDriverSwitcher::GetCurrentGameNetDriverClassName());

	if (!ResolveError.IsEmpty())
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Backend resolution note: %s"), *ResolveError);
	}

	if (Config.bAutoLoginOnInitialize)
	{
		// Off by default and it should stay off: this starts a network operation at
		// GameInstance construction, which is the behaviour commented out at CSEIKSubsystem.cpp:18.
		UE_LOG(LogCS, Warning, TEXT("[Online] bAutoLoginOnInitialize is true: logging in during GameInstance initialization."));
		Login();
	}
}

void UCSOnlineSessionSubsystem::Deinitialize()
{
	// Order matters, and there is deliberately NO early return before Super::Deinitialize.
	// The current class skips Super on any failure (CSEIKSubsystem.cpp:23-39) because it tries
	// to Logout first; do not reintroduce that call. Steam's Logout always reports failure
	// (OnlineIdentityInterfaceSteam.cpp:68) and EIK does not need one.

	// Invalidates every in-flight callback and every armed timer in one statement.
	++OpToken;
	OpState = EOpState::Idle;

	ClearOpTimeout();
	ClearAllDelegates();

	if (ActiveBackend.IsValid())
	{
		const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
		if (Session.IsValid() && Session->GetNamedSession(NAME_GameSession) != nullptr)
		{
			// Fire and forget: the GameInstance is going away, so there is nobody left to
			// receive a completion. Leaving it out is what strands zombie sessions on the
			// backend (Docs/DedicatedServer_Migration.md:373).
			UE_LOG(LogCS, Log, TEXT("[Online] Deinitialize: destroying the registered session."));
			Session->DestroySession(NAME_GameSession);
		}
	}

	CurrentSearch.Reset();
	CachedResults.Empty();

	// GEngine outlives a PIE session, so an un-restored NetDriverDefinitions mutation would
	// silently leak into the next PIE run and into unrelated automation tests.
	FCSNetDriverSwitcher::RestoreDefaults();

	ActiveBackend.Reset();
	ActiveBackendId = ECSOnlineBackend::None;

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------
// Backend
// ---------------------------------------------------------------------------------------

ECSOnlineBackend UCSOnlineSessionSubsystem::GetActiveBackend() const
{
	return ActiveBackendId;
}

FText UCSOnlineSessionSubsystem::GetActiveBackendDisplayName() const
{
	if (ActiveBackend.IsValid())
	{
		return ActiveBackend->GetDisplayName();
	}

	return NSLOCTEXT("ChronoSpaceOnline", "BackendUnresolved", "None");
}

ECSOnlineRole UCSOnlineSessionSubsystem::GetOnlineRole() const
{
	return OnlineRole;
}

void UCSOnlineSessionSubsystem::GetAvailableBackends(TArray<ECSOnlineBackend>& OutBackends) const
{
	FCSOnlineBackendRegistry::Get().GetAvailableBackends(OutBackends);
}

void UCSOnlineSessionSubsystem::GetCompiledBackends(TArray<ECSOnlineBackend>& OutBackends) const
{
	FCSOnlineBackendRegistry::Get().GetCompiledBackends(OutBackends);
}

bool UCSOnlineSessionSubsystem::CanSwitchBackend() const
{
	return OpState == EOpState::Idle
		&& !IsInSession()
		&& UCSOnlineSettings::Get().bAllowRuntimeBackendSwitch;
}

bool UCSOnlineSessionSubsystem::SetActiveBackend(ECSOnlineBackend NewBackend, FString& OutError)
{
	// HARD LIMIT: this redirects only code routed through this subsystem. Every AdvancedSessions
	// / OnlineSubsystemUtils Blueprint proxy (CreateSessionCallbackProxy,
	// FindSessionsCallbackProxyAdvanced, JoinSessionCallbackProxy, AdvancedFriendsGameInstance's
	// invite flow) resolves through Online::GetSubsystem(World) -> DefaultPlatformService and
	// keeps talking to the old backend. BP_CSGameInstance, BP_SessionUI and WBP_EOSLobby are
	// built on exactly those nodes, so a runtime switch leaves them on one backend while this
	// facade is on another. Use RequestBackendSwitchWithRestart for a switch that moves them too.
	OutError.Reset();

	if (!CanSwitchBackend())
	{
		OutError = TEXT("Cannot switch backend right now: an operation is in flight, a session is registered, or bAllowRuntimeBackendSwitch is false.");
		UE_LOG(LogCS, Warning, TEXT("[Online] SetActiveBackend refused. %s"), *OutError);
		return false;
	}

	if (NewBackend == ActiveBackendId)
	{
		return true;
	}

	const TSharedPtr<ICSOnlineBackend> Candidate = FCSOnlineBackendRegistry::Get().Find(NewBackend);
	if (!Candidate.IsValid())
	{
		OutError = FString::Printf(TEXT("Backend %s was compiled out of this build."), UCSOnlineSettings::LexToString(NewBackend));
		UE_LOG(LogCS, Warning, TEXT("[Online] SetActiveBackend refused. %s"), *OutError);
		return false;
	}

	if (!Candidate->IsAvailable())
	{
		// An explicit user request must not fall back silently -- report why instead. Both real
		// backends fail silently by default, which is the entire reason GetUnavailableReason exists.
		OutError = FString::Printf(TEXT("Backend %s is unavailable: %s"),
			UCSOnlineSettings::LexToString(NewBackend), *Candidate->GetUnavailableReason());
		UE_LOG(LogCS, Warning, TEXT("[Online] SetActiveBackend refused. %s"), *OutError);
		return false;
	}

	// Unregister from the OSS that actually holds the delegates before the swap, otherwise live
	// callbacks stay registered on the old subsystem forever.
	ClearAllDelegates();

	CurrentSearch.Reset();
	CachedResults.Empty();

	// Anything the caller is still holding from the previous backend's search is unjoinable now.
	++SearchGeneration;

	const ECSOnlineBackend OldBackend = ActiveBackendId;
	ActiveBackend = Candidate;
	ActiveBackendId = NewBackend;

	UCSOnlineSettings::Get().WritePreferredBackend(NewBackend);

	UE_LOG(LogCS, Log, TEXT("[Online] Active backend switched: %s -> %s (subsystem '%s')."),
		UCSOnlineSettings::LexToString(OldBackend),
		UCSOnlineSettings::LexToString(NewBackend),
		*ActiveBackend->GetSubsystemName().ToString());

	OnBackendChanged.Broadcast(OldBackend, NewBackend);
	return true;
}

void UCSOnlineSessionSubsystem::RequestBackendSwitchWithRestart(ECSOnlineBackend NewBackend)
{
	const TSharedPtr<ICSOnlineBackend> Candidate = FCSOnlineBackendRegistry::Get().Find(NewBackend);
	if (!Candidate.IsValid())
	{
		UE_LOG(LogCS, Error, TEXT("[Online] RequestBackendSwitchWithRestart: backend %s was compiled out of this build."),
			UCSOnlineSettings::LexToString(NewBackend));
		return;
	}

	// The subsystem name comes off the adapter, never off a literal in this file. Writing
	// DefaultPlatformService is what moves the legacy AdvancedSessions graphs too, because
	// Online::GetSubsystem(World) reads exactly this key.
	const FString SubsystemName = Candidate->GetSubsystemName().ToString();

	if (GConfig)
	{
		GConfig->SetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), *SubsystemName, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[Online] RequestBackendSwitchWithRestart: GConfig is null; DefaultPlatformService was not written."));
	}

	UCSOnlineSettings::Get().WritePreferredBackend(NewBackend);

	// Deliberately NOT IOnlineSubsystem::ReloadDefaultSubsystem(): its own header says
	// editor-only and crash-prone (OnlineSubsystemModule.h:193-197).
	UE_LOG(LogCS, Warning,
		TEXT("[Online] Backend switch to %s (subsystem '%s') written to config. A RESTART is required for it to take effect."),
		UCSOnlineSettings::LexToString(NewBackend), *SubsystemName);
}

bool UCSOnlineSessionSubsystem::ResolveBackend(ECSOnlineBackend Requested, FString& OutError)
{
	OutError.Reset();

	FCSOnlineBackendRegistry& Registry = FCSOnlineBackendRegistry::Get();

	// Keeping intent (what was asked for) separate from availability (what can actually run) is
	// what lets the log say "requested Steam, Steam unavailable (client not running), fell back
	// to EIK" instead of just reporting EIK and leaving the player to guess.
	const TSharedPtr<ICSOnlineBackend> RequestedBackend = Registry.Find(Requested);

	TSharedPtr<ICSOnlineBackend> Chosen;
	if (RequestedBackend.IsValid() && RequestedBackend->IsAvailable())
	{
		Chosen = RequestedBackend;
	}
	else
	{
		if (!RequestedBackend.IsValid())
		{
			OutError = FString::Printf(TEXT("requested %s, which was compiled out of this build"),
				UCSOnlineSettings::LexToString(Requested));
		}
		else
		{
			OutError = FString::Printf(TEXT("requested %s, which is unavailable (%s)"),
				UCSOnlineSettings::LexToString(Requested), *RequestedBackend->GetUnavailableReason());
		}

		// Try the requested backend first anyway, then the configured fallback order. The Null
		// adapter is always compiled and always available, so FindFirstAvailable cannot fail --
		// which is why this subsystem has no null-backend code path to get wrong.
		TArray<ECSOnlineBackend> PreferenceOrder;
		PreferenceOrder.Add(Requested);
		for (const ECSOnlineBackend FallbackId : UCSOnlineSettings::Get().BackendFallbackOrder)
		{
			PreferenceOrder.AddUnique(FallbackId);
		}

		Chosen = Registry.FindFirstAvailable(PreferenceOrder);
	}

	const ECSOnlineBackend OldBackend = ActiveBackendId;
	ActiveBackend = Chosen;
	ActiveBackendId = Chosen.IsValid() ? Chosen->GetBackendId() : ECSOnlineBackend::None;

	if (ActiveBackendId != Requested)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Backend fallback: %s -> %s. Reason: %s."),
			UCSOnlineSettings::LexToString(Requested),
			UCSOnlineSettings::LexToString(ActiveBackendId),
			OutError.IsEmpty() ? TEXT("unspecified") : *OutError);
	}

	if (OldBackend != ActiveBackendId)
	{
		OnBackendChanged.Broadcast(OldBackend, ActiveBackendId);
	}

	return ActiveBackend.IsValid();
}

// ---------------------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::Login()
{
	if (!ActiveBackend.IsValid())
	{
		const FString Message = MakeRejectionMessage(ECSOnlineOpResult::NoBackend, TEXT("Login"));
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		OnLoginComplete.Broadcast(false, FString(), Message);
		return;
	}

	// HostSession / FindSessions route through here after setting OpState to LoggingIn: they
	// already own the operation slot, so BeginOp would (correctly) reject them as Busy. Only
	// take a fresh slot when this is a standalone login.
	if (OpState != EOpState::LoggingIn)
	{
		ECSOnlineOpResult Rejection = ECSOnlineOpResult::Success;
		if (!BeginOp(EOpState::LoggingIn, Rejection))
		{
			const FString Message = MakeRejectionMessage(Rejection, TEXT("Login"));
			UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
			OnLoginComplete.Broadcast(false, FString(), Message);
			return;
		}
	}

	if (!ActiveBackend->NeedsExplicitLogin() || ActiveBackend->IsLoggedIn(0))
	{
		// Already authenticated by the platform (Steam, Null) or authenticated earlier. Route the
		// synchronous success through the same continuation the async path uses, so that a
		// pending HostSession / FindSessions resumes identically either way and there is exactly
		// one place that knows how to resume one.
		HandleLoginComplete(0, /*bWasSuccessful=*/ true, *FUniqueNetIdString::EmptyId(), FString());
		return;
	}

	const IOnlineIdentityPtr Identity = ActiveBackend->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		const FString Message = FString::Printf(TEXT("Login failed: backend %s has no identity interface."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::NoBackend, Message);
		OnLoginComplete.Broadcast(false, FString(), Message);
		return;
	}

	FOnlineAccountCredentials Credentials;
	if (!ActiveBackend->BuildLoginCredentials(0, Credentials))
	{
		const FString Message = FString::Printf(
			TEXT("Login failed: backend %s reports NeedsExplicitLogin() but produced no credentials."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnLoginComplete.Broadcast(false, FString(), Message);
		return;
	}

	// The handle is ALWAYS stored and ALWAYS cleared. CSEIKSubsystem.cpp:65-68 does an
	// AddUObject on every call and never stores a handle, so every repeated login attempt
	// permanently accumulates another live callback.
	LoginCompleteHandle = Identity->AddOnLoginCompleteDelegate_Handle(
		0, FOnLoginCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleLoginComplete));
	DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();

	if (!Identity->Login(0, Credentials))
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
		LoginCompleteHandle.Reset();

		const FString Message = TEXT("Login failed: the identity interface refused the request.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);

		const bool bHadPendingHost = bPendingHostAfterLogin;
		const bool bHadPendingFind = bPendingFindAfterLogin;
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnLoginComplete.Broadcast(false, FString(), Message);

		if (bHadPendingHost)
		{
			OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		}
		if (bHadPendingFind)
		{
			OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Failed, TArray<FCSSessionSearchResult>(), Message);
		}
	}
}

bool UCSOnlineSessionSubsystem::IsLoggedIn() const
{
	return ActiveBackend.IsValid() && ActiveBackend->IsLoggedIn(0);
}

FString UCSOnlineSessionSubsystem::GetLocalPlayerNickname() const
{
	if (!ActiveBackend.IsValid())
	{
		return FString();
	}

	const IOnlineIdentityPtr Identity = ActiveBackend->GetIdentityInterface();
	return Identity.IsValid() ? Identity->GetPlayerNickname(0) : FString();
}

FString UCSOnlineSessionSubsystem::GetLocalPlayerNetIdString() const
{
	if (!ActiveBackend.IsValid())
	{
		return FString();
	}

	const IOnlineIdentityPtr Identity = ActiveBackend->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		return FString();
	}

	const FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);
	return NetId.IsValid() ? NetId->ToString() : FString();
}

void UCSOnlineSessionSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (LocalUserNum != 0)
	{
		return;
	}

	// First two statements of every completion handler: null-check the interface, then clear our
	// own delegate handle. A missing identity interface here is not a failure -- there is simply
	// nothing left to unregister from.
	const IOnlineIdentityPtr Identity = ResolveIdentityInterface(DelegateOwnerSubsystem);
	if (Identity.IsValid())
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
	}
	LoginCompleteHandle.Reset();

	if (OpState != EOpState::LoggingIn)
	{
		// A completion that outlived its operation (timeout already fired, or Deinitialize ran).
		// Report it, but do not touch the operation slot somebody else now owns.
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale login completion (bWasSuccessful=%s)."),
			bWasSuccessful ? TEXT("true") : TEXT("false"));
		return;
	}

	const bool bResumeHost = bPendingHostAfterLogin;
	const bool bResumeFind = bPendingFindAfterLogin;
	bPendingHostAfterLogin = false;
	bPendingFindAfterLogin = false;

	if (!bWasSuccessful)
	{
		const FString Message = Error.IsEmpty() ? FString(TEXT("Login failed.")) : Error;
		UE_LOG(LogCS, Error, TEXT("[Online] Login failed: %s"), *Message);

		FinishOp(ECSOnlineOpResult::NotLoggedIn, Message);
		OnLoginComplete.Broadcast(false, FString(), Message);

		// Whoever was waiting on this login must be told, or the UI hangs on a spinner forever.
		if (bResumeHost)
		{
			OnHostComplete.Broadcast(ECSOnlineOpResult::NotLoggedIn, Message);
		}
		if (bResumeFind)
		{
			OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::NotLoggedIn, TArray<FCSSessionSearchResult>(), Message);
		}
		return;
	}

	const FString Nickname = GetLocalPlayerNickname();
	UE_LOG(LogCS, Log, TEXT("[Online] Login succeeded. Nickname='%s', NetId='%s'."), *Nickname, *UserId.ToString());

	OnLoginComplete.Broadcast(true, Nickname, FString());

	if (!bResumeHost && !bResumeFind)
	{
		FinishOp(ECSOnlineOpResult::Success, FString());
		return;
	}

	// Guard against a backend that reports login success while its identity interface still says
	// logged out. Resuming would re-enter the same login gate, set the pending flag again and
	// come straight back here -- an endless slow retry loop against the online service. Fail the
	// pending operation loudly instead.
	if (ActiveBackend.IsValid() && ActiveBackend->NeedsExplicitLogin() && !ActiveBackend->IsLoggedIn(0))
	{
		const FString StallMessage = FString::Printf(
			TEXT("Backend %s reported login success but its identity interface still reports logged out. Refusing to retry."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *StallMessage);

		FinishOp(ECSOnlineOpResult::NotLoggedIn, StallMessage);
		if (bResumeHost)
		{
			OnHostComplete.Broadcast(ECSOnlineOpResult::NotLoggedIn, StallMessage);
		}
		else
		{
			OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::NotLoggedIn, TArray<FCSSessionSearchResult>(), StallMessage);
		}
		return;
	}

	// Copy the pending params out before FinishOp, then re-enter the public entry point. The
	// params have already been resolved and sanitised by the first pass, so re-entry is
	// idempotent -- and the login gate is now satisfied, so it proceeds to the real work.
	const FCSHostSessionParams  ResumeHostParams = PendingHostParams;
	const FCSFindSessionsParams ResumeFindParams = PendingFindParams;

	FinishOp(ECSOnlineOpResult::Success, FString());

	if (bResumeHost)
	{
		HostSession(ResumeHostParams);
	}
	else
	{
		FindSessions(ResumeFindParams);
	}
}

// ---------------------------------------------------------------------------------------
// Host
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::HostSession(const FCSHostSessionParams& Params)
{
	ECSOnlineOpResult Rejection = ECSOnlineOpResult::Success;
	if (!BeginOp(EOpState::Creating, Rejection))
	{
		const FString Message = MakeRejectionMessage(Rejection, TEXT("HostSession"));
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		OnHostComplete.Broadcast(Rejection, Message);
		return;
	}

	OnlineRole = ECSOnlineRole::ListenHost;

	const UCSOnlineSettings& Config = UCSOnlineSettings::Get();
	PendingHostParams = Params;

	// FindEOSSession unconditionally injects a NumPublicConnections >= 1 filter
	// (OnlineSessionEOS.cpp:2415), so a session with zero public connections is invisible on the
	// EIK Sessions path. Never let a caller create one.
	if (PendingHostParams.MaxPlayers < 1)
	{
		PendingHostParams.MaxPlayers = FMath::Max(1, Config.DefaultMaxPlayers);
	}

	if (PendingHostParams.TravelLevel.IsNull())
	{
		PendingHostParams.TravelLevel = Config.DefaultHostLevel;
	}

	if (PendingHostParams.GameIdTagOverride.IsEmpty())
	{
		PendingHostParams.GameIdTagOverride = Config.GameIdTag;
	}

	if (PendingHostParams.SessionDisplayName.IsEmpty())
	{
		PendingHostParams.SessionDisplayName = GetLocalPlayerNickname();
	}

	// The travel URL, "?listen" included, is built by BuildListenTravelURL and nowhere else. A
	// caller that typed it into ExtraTravelOptions would produce "...?listen?listen", so strip it
	// here rather than trusting the call site: host travel must not be able to diverge between
	// callers. CSEIKSubsystem.cpp:164, CSLabyrinthKeyAltar.cpp:116 and SCSServerTravelWidget.cpp:89
	// each hardcode their own "?listen" today, which is exactly the drift this prevents.
	if (PendingHostParams.ExtraTravelOptions.Contains(TEXT("?listen"), ESearchCase::IgnoreCase))
	{
		const FString OriginalOptions = PendingHostParams.ExtraTravelOptions;
		PendingHostParams.ExtraTravelOptions = OriginalOptions.Replace(TEXT("?listen"), TEXT(""), ESearchCase::IgnoreCase);
		UE_LOG(LogCS, Warning,
			TEXT("[Online] HostSession stripped \"?listen\" from ExtraTravelOptions ('%s' -> '%s'). The online layer owns the travel URL; call sites must not add \"?listen\"."),
			*OriginalOptions, *PendingHostParams.ExtraTravelOptions);
	}

	if (ActiveBackend->NeedsExplicitLogin() && !IsLoggedIn())
	{
		// Keep the operation slot; only the sub-state changes. HandleLoginComplete resumes by
		// re-entering HostSession once the login lands.
		bPendingHostAfterLogin = true;
		OpState = EOpState::LoggingIn;
		Login();
		return;
	}

	const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
	if (!Session.IsValid())
	{
		const FString Message = FString::Printf(TEXT("HostSession failed: backend %s has no session interface."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::NoBackend, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::NoBackend, Message);
		return;
	}

	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		const FString Message = TEXT("HostSession failed: a session is already registered. Call LeaveSession first.");
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::AlreadyInSession, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::AlreadyInSession, Message);
		return;
	}

	// Everything backend-specific about the session shape -- lobbies vs sessions, presence,
	// advertised keys -- happens inside the adapter. UsesLobbies() is read by both
	// FillCreateSettings and FillSearchSettings, which is what removes the silent-join-failure
	// bug class structurally instead of by discipline.
	FOnlineSessionSettings SessionSettings;
	ActiveBackend->FillCreateSettings(SessionSettings, PendingHostParams, Config);

	CreateSessionCompleteHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleCreateSessionComplete));
	DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();

	UE_LOG(LogCS, Log, TEXT("[Online] CreateSession: backend=%s, name='%s', maxPlayers=%d, LAN=%s."),
		UCSOnlineSettings::LexToString(ActiveBackendId),
		*PendingHostParams.SessionDisplayName,
		PendingHostParams.MaxPlayers,
		PendingHostParams.bIsLANMatch ? TEXT("true") : TEXT("false"));

	if (!Session->CreateSession(0, NAME_GameSession, SessionSettings))
	{
		Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();

		const FString Message = TEXT("HostSession failed: CreateSession was refused by the session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
	}
}

void UCSOnlineSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (!Session.IsValid())
	{
		// The OSS went away under us. Do NOT dereference -- this is the exact spot where
		// CSEIKSubsystem.cpp:122-124 crashes when the backend is not running.
		CreateSessionCompleteHandle.Reset();

		const FString Message = TEXT("CreateSession completion arrived with no session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	CreateSessionCompleteHandle.Reset();

	if (OpState != EOpState::Creating)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale CreateSession completion for '%s'."), *SessionName.ToString());
		return;
	}

	if (!bWasSuccessful)
	{
		const FString Message = FString::Printf(TEXT("CreateSession failed for '%s'."), *SessionName.ToString());
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	UE_LOG(LogCS, Log, TEXT("[Online] CreateSession succeeded for '%s'. Starting the session."), *SessionName.ToString());

	OpState = EOpState::Starting;

	// Re-arm the watchdog for the next async leg. SetTimer clears the timer already bound to
	// this handle, and OpToken is unchanged, so the armed timer stays valid for this operation.
	StartOpTimeout();

	StartSessionCompleteHandle = Session->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleStartSessionComplete));
	if (ActiveBackend.IsValid())
	{
		DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();
	}

	if (!Session->StartSession(NAME_GameSession))
	{
		Session->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
		StartSessionCompleteHandle.Reset();

		const FString Message = TEXT("StartSession was refused by the session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
	}
}

void UCSOnlineSessionSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// SESSION START AND TRAVEL STAY COUPLED, AND IN THIS ORDER
	// (Docs/DedicatedServer_Migration.md:708). ServerTravel fires from here and is never
	// decoupled into a separate caller-driven step: CreateSession must follow the first map load
	// or the NetDriver is not picked up.
	//
	// NEVER insert a DestroySession before this travel. ACSGameMode::Logout releases a player
	// slot only while GetWorld()->bIsTearingDown (CSGameMode.cpp:138-145), so a disconnect
	// outside a tearing-down world makes every connected player lose their slot mid-travel and
	// re-roll their character.
	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (!Session.IsValid())
	{
		StartSessionCompleteHandle.Reset();

		const FString Message = TEXT("StartSession completion arrived with no session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	Session->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
	StartSessionCompleteHandle.Reset();

	if (OpState != EOpState::Starting)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale StartSession completion for '%s'."), *SessionName.ToString());
		return;
	}

	if (!bWasSuccessful)
	{
		const FString Message = FString::Printf(TEXT("StartSession failed for '%s'."), *SessionName.ToString());
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnHostComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	PendingTravelURL = BuildListenTravelURL(PendingHostParams);

	if (ActiveBackend.IsValid())
	{
		FString SwitchError;
		if (!FCSNetDriverSwitcher::ApplyForBackend(*ActiveBackend, GetWorld(), SwitchError))
		{
			// Warning, not a failure. Aborting here would leave a live, advertised session with no
			// way in. CreateNetDriver_Local falls back to DriverClassNameFallback when the class
			// fails to load or its CDO reports !IsAvailable() (UnrealEngine.cpp:15020-15024), so a
			// wrong driver degrades to IpNetDriver rather than breaking travel.
			UE_LOG(LogCS, Warning, TEXT("[Online] Net driver switch skipped before ServerTravel: %s"), *SwitchError);
		}
	}

	// Traveling carries no watchdog on purpose: a timeout firing across a ServerTravel would be
	// worse than the hang it guards against.
	ClearOpTimeout();
	OpState = EOpState::Traveling;

	// Broadcast BEFORE travelling, so UMG can close the menu while the world still exists.
	OnHostComplete.Broadcast(ECSOnlineOpResult::Success, FString());

	DoServerTravel();
}

FString UCSOnlineSessionSubsystem::BuildListenTravelURL(const FCSHostSessionParams& Params) const
{
	const UCSOnlineSettings& Config = UCSOnlineSettings::Get();
	const TSoftObjectPtr<UWorld> Level = Params.TravelLevel.IsNull() ? Config.DefaultHostLevel : Params.TravelLevel;

	FString URL;
	if (!Level.IsNull())
	{
		// The long package name, not the full asset path: ServerTravel wants
		// /Game/02_Map/L_StageSize, not /Game/02_Map/L_StageSize.L_StageSize.
		URL = Level.ToSoftObjectPath().GetLongPackageName();
	}

	if (URL.IsEmpty())
	{
		// No level configured anywhere. Travel to the current map so the host at least keeps a
		// world and the listen driver still comes up.
		const UWorld* World = GetWorld();
		URL = World ? World->GetOutermost()->GetName() : FString();
		UE_LOG(LogCS, Error,
			TEXT("[Online] No host level configured (FCSHostSessionParams::TravelLevel and UCSOnlineSettings::DefaultHostLevel are both empty). Falling back to the current map '%s'."),
			*URL);
	}

	FString ExtraOptions = Params.ExtraTravelOptions;
	ExtraOptions.TrimStartAndEndInline();
	if (!ExtraOptions.IsEmpty())
	{
		if (!ExtraOptions.StartsWith(TEXT("?")) && !ExtraOptions.StartsWith(TEXT("#")))
		{
			URL += TEXT("?");
		}
		URL += ExtraOptions;
	}

	// THE ONLY PLACE "?listen" IS EVER WRITTEN.
	URL += TEXT("?listen");

	if (ActiveBackend.IsValid())
	{
		ActiveBackend->DecorateListenURL(URL);
	}

	return URL;
}

void UCSOnlineSessionSubsystem::DoServerTravel()
{
	UWorld* World = GetWorld();
	if (World && World->GetAuthGameMode())
	{
		UE_LOG(LogCS, Log, TEXT("[Online] ServerTravel -> %s"), *PendingTravelURL);
		World->ServerTravel(PendingTravelURL, false);
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[Online] ServerTravel skipped: no authoritative game mode. URL was '%s'."), *PendingTravelURL);
	}

	FinishOp(ECSOnlineOpResult::Success, FString());
}

// ---------------------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::FindSessions(const FCSFindSessionsParams& Params)
{
	ECSOnlineOpResult Rejection = ECSOnlineOpResult::Success;
	if (!BeginOp(EOpState::Finding, Rejection))
	{
		const FString Message = MakeRejectionMessage(Rejection, TEXT("FindSessions"));
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		OnFindSessionsComplete.Broadcast(Rejection, TArray<FCSSessionSearchResult>(), Message);
		return;
	}

	// Every previously handed-out FCSSessionSearchResult becomes unjoinable the moment a new
	// search starts, so the counter moves first and JoinSessionByResult compares against it.
	++SearchGeneration;
	CachedResults.Reset();
	PendingFindParams = Params;

	if (ActiveBackend->NeedsExplicitLogin() && !IsLoggedIn())
	{
		bPendingFindAfterLogin = true;
		OpState = EOpState::LoggingIn;
		Login();
		return;
	}

	const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
	if (!Session.IsValid())
	{
		const FString Message = FString::Printf(TEXT("FindSessions failed: backend %s has no session interface."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::NoBackend, Message);
		OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::NoBackend, CachedResults, Message);
		return;
	}

	// The search is owned by this subsystem and stays alive: FCSSessionSearchResult carries an
	// INDEX into it, never a copy of the raw result, because EIK's lobby join needs the entry to
	// still be present in its own cache (OnlineSessionEOS.cpp:4068-4076).
	CurrentSearch = MakeShared<FOnlineSessionSearch>();
	ActiveBackend->FillSearchSettings(*CurrentSearch, PendingFindParams, UCSOnlineSettings::Get());

	FindSessionsCompleteHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleFindSessionsComplete));
	DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();

	UE_LOG(LogCS, Log, TEXT("[Online] FindSessions: backend=%s, generation=%d, maxResults=%d, LAN=%s."),
		UCSOnlineSettings::LexToString(ActiveBackendId),
		SearchGeneration,
		PendingFindParams.MaxResults,
		PendingFindParams.bLANQuery ? TEXT("true") : TEXT("false"));

	if (!Session->FindSessions(0, CurrentSearch.ToSharedRef()))
	{
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		CurrentSearch.Reset();

		const FString Message = TEXT("FindSessions was refused by the session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Failed, CachedResults, Message);
	}
}

void UCSOnlineSessionSubsystem::CancelFindSessions()
{
	if (OpState != EOpState::Finding)
	{
		UE_LOG(LogCS, Verbose, TEXT("[Online] CancelFindSessions ignored: no search is in flight."));
		return;
	}

	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (Session.IsValid())
	{
		// Clear our completion delegate FIRST. Backends differ on whether a cancelled search
		// still fires OnFindSessionsComplete, and a late one arriving after we have already
		// reported Cancelled would broadcast a second, contradictory result.
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		Session->CancelFindSessions();
	}
	FindSessionsCompleteHandle.Reset();

	// Whatever that search would have returned is unusable, so retire the generation with it.
	++SearchGeneration;
	CurrentSearch.Reset();
	CachedResults.Reset();

	const FString Message = TEXT("Search cancelled.");
	FinishOp(ECSOnlineOpResult::Cancelled, Message);
	OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Cancelled, CachedResults, Message);
}

void UCSOnlineSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (!Session.IsValid())
	{
		FindSessionsCompleteHandle.Reset();

		const FString Message = TEXT("FindSessions completion arrived with no session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Failed, CachedResults, Message);
		return;
	}

	Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	FindSessionsCompleteHandle.Reset();

	if (OpState != EOpState::Finding)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale FindSessions completion."));
		return;
	}

	// This handler NEVER auto-joins. CSEIKSubsystem.cpp:206-212 joins the first result and
	// breaks, which makes a session list impossible to build; picking a result is the caller's
	// decision and arrives back through JoinSessionByResult.
	RebuildCachedResults();

	const ECSOnlineOpResult Result = bWasSuccessful ? ECSOnlineOpResult::Success : ECSOnlineOpResult::Failed;
	const FString Message = bWasSuccessful ? FString() : FString(TEXT("FindSessions reported failure."));

	if (!bWasSuccessful)
	{
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
	}

	FinishOp(Result, Message);
	OnFindSessionsComplete.Broadcast(Result, CachedResults, Message);
}

void UCSOnlineSessionSubsystem::RebuildCachedResults()
{
	CachedResults.Reset();

	if (!CurrentSearch.IsValid() || !ActiveBackend.IsValid())
	{
		return;
	}

	const UCSOnlineSettings& Config = UCSOnlineSettings::Get();

	int32 RejectedCount = 0;
	for (int32 Index = 0; Index < CurrentSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Raw = CurrentSearch->SearchResults[Index];
		if (!Raw.IsValid() || !ActiveBackend->ShouldAcceptSearchResult(Raw, PendingFindParams, Config))
		{
			++RejectedCount;
			continue;
		}

		const FOnlineSessionSettings& RawSettings = Raw.Session.SessionSettings;

		FCSSessionSearchResult& Out = CachedResults.AddDefaulted_GetRef();
		Out.ResultIndex = Index;
		Out.SearchGeneration = SearchGeneration;
		Out.Backend = ActiveBackendId;
		Out.OwningPlayerName = Raw.Session.OwningUserName;
		Out.SessionIdString = Raw.GetSessionIdStr();
		Out.PingInMs = Raw.PingInMs;
		Out.MaxPlayers = RawSettings.NumPublicConnections + RawSettings.NumPrivateConnections;
		Out.OpenSlots = Raw.Session.NumOpenPublicConnections;
		Out.CurrentPlayers = FMath::Max(0, Out.MaxPlayers - Out.OpenSlots);
		Out.bIsLAN = RawSettings.bIsLANMatch;
		Out.bAllowJoinInProgress = RawSettings.bAllowJoinInProgress;
		Out.bIsFull = Out.OpenSlots <= 0;

		RawSettings.Get(Config.SessionNameKey, Out.SessionDisplayName);
		RawSettings.Get(SETTING_MAPNAME, Out.MapName);

		if (Out.SessionDisplayName.IsEmpty())
		{
			Out.SessionDisplayName = Out.OwningPlayerName;
		}
	}

	// "found 40, kept 0" and "found 0" are completely different diagnoses: the first is a
	// client-side filter problem (game-id tag or build id), the second is a query or backend
	// problem. Logging both counts is what tells them apart without a debugger.
	UE_LOG(LogCS, Log, TEXT("[Online] FindSessions results: %d raw, kept %d, rejected %d (generation %d)."),
		CurrentSearch->SearchResults.Num(), CachedResults.Num(), RejectedCount, SearchGeneration);
}

void UCSOnlineSessionSubsystem::GetLastSearchResults(TArray<FCSSessionSearchResult>& Out) const
{
	Out = CachedResults;
}

// ---------------------------------------------------------------------------------------
// Join
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::JoinSessionByResult(const FCSSessionSearchResult& Result)
{
	if (Result.SearchGeneration != SearchGeneration)
	{
		// EIK's JoinLobbySession requires the lobby to still be present in
		// LobbySearchResultsCache, keyed by session-id string (OnlineSessionEOS.cpp:4068-4076),
		// and that cache is wiped at the start of every StartLobbySearch (:4526). A result from a
		// superseded search therefore cannot be joined at all -- failing here with a clear line
		// beats failing deep inside EOS with none.
		const FString Message = FString::Printf(
			TEXT("Search superseded: this result is from search generation %d, the current generation is %d. Search again before joining."),
			Result.SearchGeneration, SearchGeneration);
		UE_LOG(LogCS, Warning, TEXT("[Online] JoinSession refused. %s"), *Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::InvalidSession, Message);
		return;
	}

	if (Result.Backend != ActiveBackendId)
	{
		// A result found on one backend cannot be joined on another: the session id, the connect
		// string format and the join routing are all backend-specific.
		const FString Message = FString::Printf(TEXT("This result belongs to backend %s, but the active backend is %s."),
			UCSOnlineSettings::LexToString(Result.Backend), UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Warning, TEXT("[Online] JoinSession refused. %s"), *Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::InvalidSession, Message);
		return;
	}

	JoinSessionByIndex(Result.ResultIndex);
}

void UCSOnlineSessionSubsystem::JoinSessionByIndex(int32 ResultIndex)
{
	if (!CurrentSearch.IsValid() || !CurrentSearch->SearchResults.IsValidIndex(ResultIndex))
	{
		const FString Message = FString::Printf(TEXT("Result index %d is out of range (%d result(s) available)."),
			ResultIndex, CurrentSearch.IsValid() ? CurrentSearch->SearchResults.Num() : 0);
		UE_LOG(LogCS, Warning, TEXT("[Online] JoinSession refused. %s"), *Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::InvalidSession, Message);
		return;
	}

	JoinSessionNative(CurrentSearch->SearchResults[ResultIndex]);
}

void UCSOnlineSessionSubsystem::JoinSessionNative(const FOnlineSessionSearchResult& SearchResult)
{
	ECSOnlineOpResult Rejection = ECSOnlineOpResult::Success;
	if (!BeginOp(EOpState::Joining, Rejection))
	{
		const FString Message = MakeRejectionMessage(Rejection, TEXT("JoinSession"));
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		OnJoinSessionComplete.Broadcast(Rejection, Message);
		return;
	}

	const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
	if (!Session.IsValid())
	{
		const FString Message = FString::Printf(TEXT("JoinSession failed: backend %s has no session interface."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::NoBackend, Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::NoBackend, Message);
		return;
	}

	if (Session->GetNamedSession(NAME_GameSession) != nullptr)
	{
		const FString Message = TEXT("JoinSession failed: a session is already registered. Call LeaveSession first.");
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::AlreadyInSession, Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::AlreadyInSession, Message);
		return;
	}

	// The net driver is switched BEFORE JoinSession, so the transport is already correct by the
	// time GetResolvedConnectString hands us an address that only that transport can dial.
	FString SwitchError;
	if (!FCSNetDriverSwitcher::ApplyForBackend(*ActiveBackend, GetWorld(), SwitchError))
	{
		// Warning, not a failure: the engine falls back to DriverClassNameFallback when the class
		// fails to load or its CDO reports !IsAvailable() (UnrealEngine.cpp:15020-15024).
		UE_LOG(LogCS, Warning, TEXT("[Online] Net driver switch skipped before JoinSession: %s"), *SwitchError);
	}

	JoinSessionCompleteHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleJoinSessionComplete));
	DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();

	UE_LOG(LogCS, Log, TEXT("[Online] JoinSession: backend=%s, sessionId='%s'."),
		UCSOnlineSettings::LexToString(ActiveBackendId), *SearchResult.GetSessionIdStr());

	if (!Session->JoinSession(0, NAME_GameSession, SearchResult))
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();

		const FString Message = TEXT("JoinSession was refused by the session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
	}
}

void UCSOnlineSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (!Session.IsValid())
	{
		// CSEIKSubsystem.cpp:251-253 dereferences Get() and GetSessionInterface() here unchecked.
		JoinSessionCompleteHandle.Reset();

		const FString Message = TEXT("JoinSession completion arrived with no session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	JoinSessionCompleteHandle.Reset();

	if (OpState != EOpState::Joining)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale JoinSession completion for '%s'."), *SessionName.ToString());
		return;
	}

	// THE RESULT IS CHECKED FIRST, BEFORE ANY TRAVEL. CSEIKSubsystem.cpp:255-265 travels first
	// and inspects Result afterwards, which happily ClientTravels on a failed join.
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		const ECSOnlineOpResult MappedResult = MapJoinResult(Result);
		const FString Message = FString::Printf(TEXT("JoinSession failed for '%s': %s."),
			*SessionName.ToString(), JoinResultToString(Result));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(MappedResult, Message);
		OnJoinSessionComplete.Broadcast(MappedResult, Message);
		return;
	}

	FString ConnectInfo;
	if (!Session->GetResolvedConnectString(SessionName, ConnectInfo) || ConnectInfo.IsEmpty())
	{
		const FString Message = FString::Printf(TEXT("JoinSession succeeded for '%s' but GetResolvedConnectString produced no address."),
			*SessionName.ToString());
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	// ConnectInfo is EOS:<ProductUserId>:<SocketName>:<Channel> on EIK and
	// steam.<SteamID64>:<port> on Steam. It is the single most backend-divergent value in the
	// system, it is produced by exactly one GetResolvedConnectString call, and it is passed to
	// ClientTravel VERBATIM -- never parsed, split, rebuilt or logged-and-reconstructed. Keeping
	// it opaque is precisely what lets one shared flow serve both backends.
	if (ActiveBackend.IsValid())
	{
		ActiveBackend->DecorateClientTravelURL(ConnectInfo);
	}

	// No watchdog across travel, same reason as the host path.
	ClearOpTimeout();
	OpState = EOpState::Traveling;

	// Broadcast before travelling so UMG can close the menu while the world still exists.
	OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::Success, FString());

	DoClientTravel(ConnectInfo);
}

void UCSOnlineSessionSubsystem::DoClientTravel(const FString& ConnectString)
{
	APlayerController* PlayerController = GetLocalPlayerControllerChecked();
	if (!PlayerController)
	{
		const FString Message = FString::Printf(TEXT("ClientTravel skipped: no local player controller. Connect string was '%s'."), *ConnectString);
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		return;
	}

	UE_LOG(LogCS, Log, TEXT("[Online] ClientTravel -> %s"), *ConnectString);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);

	FinishOp(ECSOnlineOpResult::Success, FString());
}

// ---------------------------------------------------------------------------------------
// Leave
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::LeaveSession()
{
	// This is the half of the session lifecycle that does not exist anywhere in the project
	// today; without it, sessions are left registered on the backend
	// (Docs/DedicatedServer_Migration.md:373).
	//
	// It must never be folded into the host or join travel sequence. ACSGameMode::Logout
	// releases a player slot only while GetWorld()->bIsTearingDown (CSGameMode.cpp:138-145), so
	// destroying the session outside a teardown disconnects everyone and costs them their slots.
	ECSOnlineOpResult Rejection = ECSOnlineOpResult::Success;
	if (!BeginOp(EOpState::Destroying, Rejection))
	{
		const FString Message = MakeRejectionMessage(Rejection, TEXT("LeaveSession"));
		UE_LOG(LogCS, Warning, TEXT("[Online] %s"), *Message);
		OnLeaveSessionComplete.Broadcast(Rejection, Message);
		return;
	}

	const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
	if (!Session.IsValid())
	{
		const FString Message = FString::Printf(TEXT("LeaveSession failed: backend %s has no session interface."),
			UCSOnlineSettings::LexToString(ActiveBackendId));
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::NoBackend, Message);
		OnLeaveSessionComplete.Broadcast(ECSOnlineOpResult::NoBackend, Message);
		return;
	}

	if (Session->GetNamedSession(NAME_GameSession) == nullptr)
	{
		// Nothing to leave. Report success so callers can call this unconditionally on the way
		// back to the title screen.
		UE_LOG(LogCS, Verbose, TEXT("[Online] LeaveSession: no session was registered."));
		FinishOp(ECSOnlineOpResult::Success, FString());
		OnLeaveSessionComplete.Broadcast(ECSOnlineOpResult::Success, FString());
		return;
	}

	DestroySessionCompleteHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleDestroySessionComplete));
	DelegateOwnerSubsystem = ActiveBackend->GetSubsystemName();

	UE_LOG(LogCS, Log, TEXT("[Online] LeaveSession: destroying the registered session."));

	if (!Session->DestroySession(NAME_GameSession))
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();

		const FString Message = TEXT("DestroySession was refused by the session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnLeaveSessionComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
	}
}

void UCSOnlineSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	const IOnlineSessionPtr Session = ResolveSessionInterface(DelegateOwnerSubsystem);
	if (!Session.IsValid())
	{
		DestroySessionCompleteHandle.Reset();

		const FString Message = TEXT("DestroySession completion arrived with no session interface.");
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
		FinishOp(ECSOnlineOpResult::Failed, Message);
		OnLeaveSessionComplete.Broadcast(ECSOnlineOpResult::Failed, Message);
		return;
	}

	Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	DestroySessionCompleteHandle.Reset();

	if (OpState != EOpState::Destroying)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Ignoring a stale DestroySession completion for '%s'."), *SessionName.ToString());
		return;
	}

	if (OnlineRole == ECSOnlineRole::ListenHost)
	{
		OnlineRole = ECSOnlineRole::Client;
	}

	// Everything the caller still holds from the last search refers to a session we have just
	// left; retire the generation so a stale join is refused rather than half-attempted.
	++SearchGeneration;
	CurrentSearch.Reset();
	CachedResults.Reset();

	const ECSOnlineOpResult Result = bWasSuccessful ? ECSOnlineOpResult::Success : ECSOnlineOpResult::Failed;
	const FString Message = bWasSuccessful
		? FString()
		: FString::Printf(TEXT("DestroySession failed for '%s'."), *SessionName.ToString());

	if (bWasSuccessful)
	{
		UE_LOG(LogCS, Log, TEXT("[Online] LeaveSession complete for '%s'."), *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);
	}

	FinishOp(Result, Message);
	OnLeaveSessionComplete.Broadcast(Result, Message);
}

// ---------------------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------------------

bool UCSOnlineSessionSubsystem::IsInSession() const
{
	if (!ActiveBackend.IsValid())
	{
		return false;
	}

	const IOnlineSessionPtr Session = ActiveBackend->GetSessionInterface();
	return Session.IsValid() && Session->GetNamedSession(NAME_GameSession) != nullptr;
}

bool UCSOnlineSessionSubsystem::IsBusy() const
{
	return OpState != EOpState::Idle;
}

// ---------------------------------------------------------------------------------------
// Delegate / operation plumbing
// ---------------------------------------------------------------------------------------

void UCSOnlineSessionSubsystem::ClearAllDelegates()
{
	// Delegates are cleared against DelegateOwnerSubsystem, NOT against ActiveBackend: after a
	// backend switch the two differ, and unregistering from the new subsystem would leave live
	// callbacks registered on the old one forever.
	if (IOnlineSubsystem* Subsystem = DelegateOwnerSubsystem.IsNone() ? nullptr : IOnlineSubsystem::Get(DelegateOwnerSubsystem))
	{
		const IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid())
		{
			Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			Session->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
			Session->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
			Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		}

		const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
		}
	}

	// Reset unconditionally: if the subsystem or an interface has already gone, the handles are
	// dead anyway and must not be reused.
	LoginCompleteHandle.Reset();
	CreateSessionCompleteHandle.Reset();
	StartSessionCompleteHandle.Reset();
	FindSessionsCompleteHandle.Reset();
	JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset();

	DelegateOwnerSubsystem = NAME_None;
}

bool UCSOnlineSessionSubsystem::BeginOp(EOpState NewState, ECSOnlineOpResult& OutRejection)
{
	// Exactly one operation in flight at a time. This is what makes the unbounded delegate
	// accumulation at CSEIKSubsystem.cpp:65-68 structurally impossible rather than merely
	// unlikely.
	if (OpState != EOpState::Idle)
	{
		OutRejection = ECSOnlineOpResult::Busy;
		return false;
	}

	if (!ActiveBackend.IsValid())
	{
		OutRejection = ECSOnlineOpResult::NoBackend;
		return false;
	}

	OpState = NewState;
	++OpToken;
	StartOpTimeout();

	OutRejection = ECSOnlineOpResult::Success;
	return true;
}

void UCSOnlineSessionSubsystem::FinishOp(ECSOnlineOpResult Result, const FString& ErrorMessage)
{
	ClearOpTimeout();
	OpState = EOpState::Idle;

	// Any callback or timer still holding the old token is now inert.
	++OpToken;

	bPendingHostAfterLogin = false;
	bPendingFindAfterLogin = false;

	if (Result == ECSOnlineOpResult::Success)
	{
		UE_LOG(LogCS, Verbose, TEXT("[Online] Operation finished: Success."));
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Operation finished: %s. %s"), OpResultToString(Result), *ErrorMessage);
	}

	// Broadcasting is left to the caller: the six completion delegates have different signatures
	// and only the caller knows which one this operation belongs to.
}

void UCSOnlineSessionSubsystem::StartOpTimeout()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	const float TimeoutSeconds = UCSOnlineSettings::Get().OperationTimeoutSeconds;
	if (TimeoutSeconds <= 0.0f)
	{
		return;
	}

	// The GameInstance's timer manager, not the world's: a world timer would be destroyed along
	// with the world on travel, and this watchdog has to outlive a map change.
	// SetTimer clears whatever timer is already bound to this handle, so re-arming between the
	// legs of one operation is safe and keeps the captured token valid.
	const int32 ExpectedToken = OpToken;
	GameInstance->GetTimerManager().SetTimer(
		OpTimeoutHandle,
		FTimerDelegate::CreateUObject(this, &UCSOnlineSessionSubsystem::HandleOpTimeout, ExpectedToken),
		TimeoutSeconds,
		/*InbLoop=*/ false);
}

void UCSOnlineSessionSubsystem::ClearOpTimeout()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(OpTimeoutHandle);
	}

	OpTimeoutHandle.Invalidate();
}

void UCSOnlineSessionSubsystem::HandleOpTimeout(int32 ExpectedToken)
{
	// A superseded operation's timer becomes harmless here rather than at every call site.
	if (ExpectedToken != OpToken)
	{
		return;
	}

	const EOpState TimedOutState = OpState;
	const bool bWasPendingHost = bPendingHostAfterLogin;
	const bool bWasPendingFind = bPendingFindAfterLogin;

	const FString Message = FString::Printf(TEXT("Online operation timed out after %.1f seconds."),
		UCSOnlineSettings::Get().OperationTimeoutSeconds);
	UE_LOG(LogCS, Error, TEXT("[Online] %s"), *Message);

	// Unregister everything before reporting: whatever the backend eventually does with the
	// abandoned request must not reach us and contradict the Timeout we are about to broadcast.
	ClearAllDelegates();
	FinishOp(ECSOnlineOpResult::Timeout, Message);

	switch (TimedOutState)
	{
	case EOpState::LoggingIn:
		OnLoginComplete.Broadcast(false, FString(), Message);
		if (bWasPendingHost)
		{
			OnHostComplete.Broadcast(ECSOnlineOpResult::Timeout, Message);
		}
		if (bWasPendingFind)
		{
			OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Timeout, TArray<FCSSessionSearchResult>(), Message);
		}
		break;

	case EOpState::Creating:
	case EOpState::Starting:
		OnHostComplete.Broadcast(ECSOnlineOpResult::Timeout, Message);
		break;

	case EOpState::Finding:
		OnFindSessionsComplete.Broadcast(ECSOnlineOpResult::Timeout, TArray<FCSSessionSearchResult>(), Message);
		break;

	case EOpState::Joining:
		OnJoinSessionComplete.Broadcast(ECSOnlineOpResult::Timeout, Message);
		break;

	case EOpState::Destroying:
		OnLeaveSessionComplete.Broadcast(ECSOnlineOpResult::Timeout, Message);
		break;

	default:
		// Traveling carries no watchdog, and Idle cannot reach here because FinishOp bumps the
		// token before it sets Idle.
		break;
	}
}

APlayerController* UCSOnlineSessionSubsystem::GetLocalPlayerControllerChecked() const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	// GetFirstLocalPlayerController, not UGameplayStatics::GetPlayerController(World, 0)
	// (CSEIKSubsystem.cpp:258): index 0 of the world's controller list is whichever controller
	// was created first, which on a listen server or a split-screen setup need not be a locally
	// controlled one -- and ClientTravel on a remote controller does nothing useful.
	return GameInstance->GetFirstLocalPlayerController(GetWorld());
}

ECSOnlineOpResult UCSOnlineSessionSubsystem::MapJoinResult(EOnJoinSessionCompleteResult::Type Result)
{
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:                 return ECSOnlineOpResult::Success;
	case EOnJoinSessionCompleteResult::SessionIsFull:           return ECSOnlineOpResult::SessionFull;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:     return ECSOnlineOpResult::InvalidSession;
	case EOnJoinSessionCompleteResult::AlreadyInSession:        return ECSOnlineOpResult::AlreadyInSession;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: return ECSOnlineOpResult::Failed;
	case EOnJoinSessionCompleteResult::UnknownError:            return ECSOnlineOpResult::Failed;
	default:                                                    return ECSOnlineOpResult::Failed;
	}
}
