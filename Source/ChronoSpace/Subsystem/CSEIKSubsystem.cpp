// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSEIKSubsystem.h"

#include "Engine/GameInstance.h"
#include "OnlineSubsystemUtils.h"
#include "Subsystem/Online/CSOnlineSessionSubsystem.h"
#include "Subsystem/Online/CSOnlineTypes.h"
#include "ChronoSpace.h"

namespace
{
	/** One shared warning body so every forwarder points at the same replacement doc. */
	void LogDeprecated(const TCHAR* Old, const TCHAR* New)
	{
		UE_LOG(LogCS, Warning,
			TEXT("[CSEIKSubsystem] %s is deprecated and forwards to UCSOnlineSessionSubsystem::%s. ")
			TEXT("Re-wire the caller onto CSOnlineSessionSubsystem; this shim will be removed with the EIK backend."),
			Old, New);
	}
}

void UCSEIKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// The old implementation had `return;` here, BEFORE Super::Initialize, so this subsystem
	// never initialised at all. Nothing else belongs in this function now - the real online
	// layer initialises in UCSOnlineSessionSubsystem.
	Super::Initialize(Collection);
}

void UCSEIKSubsystem::Deinitialize()
{
	// The old implementation called Identity->Logout(0) and early-returned on failure, which
	// skipped Super::Deinitialize(). Session and identity teardown is UCSOnlineSessionSubsystem's
	// job; doing it here too would tear down a backend that subsystem still owns.
	Super::Deinitialize();
}

UCSOnlineSessionSubsystem* UCSEIKSubsystem::GetOnline() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCSOnlineSessionSubsystem>() : nullptr;
}

void UCSEIKSubsystem::LoginWithDeviceId()
{
	LogDeprecated(TEXT("LoginWithDeviceId()"), TEXT("Login()"));

	if (UCSOnlineSessionSubsystem* Online = GetOnline())
	{
		Online->Login();
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[CSEIKSubsystem] LoginWithDeviceId: UCSOnlineSessionSubsystem is unavailable."));
	}
}

void UCSEIKSubsystem::CreateSession()
{
	LogDeprecated(TEXT("CreateSession()"), TEXT("HostSession()"));

	if (UCSOnlineSessionSubsystem* Online = GetOnline())
	{
		// Defaults now come from UCSOnlineSettings, so this hosts a configurable-size session on
		// the configured map instead of the old hardcoded 2 players on /Game/02_Map/L_StageSize.
		Online->HostSession(FCSHostSessionParams());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[CSEIKSubsystem] CreateSession: UCSOnlineSessionSubsystem is unavailable."));
	}
}

void UCSEIKSubsystem::FindSessions()
{
	LogDeprecated(TEXT("FindSessions()"), TEXT("FindSessions()"));

	if (UCSOnlineSessionSubsystem* Online = GetOnline())
	{
		// The old version auto-joined the first result and broke out of the loop, so a caller
		// could never present a list. Results are now delivered via OnFindSessionsComplete.
		Online->FindSessions(FCSFindSessionsParams());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[CSEIKSubsystem] FindSessions: UCSOnlineSessionSubsystem is unavailable."));
	}
}

void UCSEIKSubsystem::JoinSessionForBlueprint(FBlueprintSessionResult& SearchResult)
{
	LogDeprecated(TEXT("JoinSessionForBlueprint()"), TEXT("JoinSessionByResult()"));

	// This function previously had an EMPTY body - any Blueprint wired to it silently did nothing.
	if (UCSOnlineSessionSubsystem* Online = GetOnline())
	{
		Online->JoinSessionNative(SearchResult.OnlineResult);
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[CSEIKSubsystem] JoinSessionForBlueprint: UCSOnlineSessionSubsystem is unavailable."));
	}
}

void UCSEIKSubsystem::JoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	LogDeprecated(TEXT("JoinSession()"), TEXT("JoinSessionNative()"));

	if (UCSOnlineSessionSubsystem* Online = GetOnline())
	{
		Online->JoinSessionNative(SearchResult);
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[CSEIKSubsystem] JoinSession: UCSOnlineSessionSubsystem is unavailable."));
	}
}
