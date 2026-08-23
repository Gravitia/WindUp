// Fill out your copyright notice in the Description page of Project Settings.

// Console commands for driving the dual-backend online layer without any UI.
//
// These are not a convenience. Steam OSS is FORCE-DISABLED inside the editor process
// (FOnlineSubsystemSteam::IsEnabled() is `IsRunningDedicatedServer() || IsRunningGame()` under
// #if UE_EDITOR, OnlineSubsystemSteam.cpp:479-484), so PIE can never exercise the Steam path.
// Every Steam test is therefore a Standalone or packaged process, and until the UMG is migrated
// off the EIK-native lobby nodes these commands are the only way to drive create/join there.
//
// All output goes through LogCS so it lands in the same category as the rest of the online layer
// and is reachable through the MCP LogsToolset.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Settings/CSOnlineSettings.h"
#include "Subsystem/Online/CSNetDriverSwitcher.h"
#include "Subsystem/Online/CSOnlineBackend.h"
#include "Subsystem/Online/CSOnlineBackendRegistry.h"
#include "Subsystem/Online/CSOnlineSessionSubsystem.h"
#include "Subsystem/Online/CSOnlineTypes.h"
#include "ChronoSpace.h"

namespace CSOnlineDebug
{
	/**
	 * Resolve the façade from the command's world, falling back to the first world context that
	 * has a GameInstance. The fallback matters because console commands can be issued from a
	 * world (e.g. a transition level) whose GameInstance pointer is momentarily null.
	 */
	static UCSOnlineSessionSubsystem* GetOnline(UWorld* World)
	{
		if (World)
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UCSOnlineSessionSubsystem* Sub = GI->GetSubsystem<UCSOnlineSessionSubsystem>())
				{
					return Sub;
				}
			}
		}

		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (UGameInstance* GI = Context.OwningGameInstance)
				{
					if (UCSOnlineSessionSubsystem* Sub = GI->GetSubsystem<UCSOnlineSessionSubsystem>())
					{
						return Sub;
					}
				}
			}
		}

		UE_LOG(LogCS, Error, TEXT("[cs.Online] No UCSOnlineSessionSubsystem found (no GameInstance yet?)."));
		return nullptr;
	}

	static const TCHAR* BackendName(ECSOnlineBackend Backend)
	{
		return UCSOnlineSettings::LexToString(Backend);
	}

	static void CmdStatus(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Ar*/)
	{
		UE_LOG(LogCS, Display, TEXT("=== cs.Online.Status ==="));
		UE_LOG(LogCS, Display, TEXT("  compiled-in: CS_WITH_EIK=%d CS_WITH_STEAM=%d CS_WITH_STEAMSOCKETS=%d"),
			(int32)CS_WITH_EIK, (int32)CS_WITH_STEAM, (int32)CS_WITH_STEAMSOCKETS);

		const UCSOnlineSettings& Settings = UCSOnlineSettings::Get();
		UE_LOG(LogCS, Display, TEXT("  settings: DefaultBackend=%s  RuntimeSwitch=%s  ManageNetDriver=%s (PIE=%s)"),
			BackendName(Settings.DefaultBackend),
			Settings.bAllowRuntimeBackendSwitch ? TEXT("true") : TEXT("false"),
			Settings.bManageNetDriverDefinitions ? TEXT("true") : TEXT("false"),
			Settings.bManageNetDriverInPIE ? TEXT("true") : TEXT("false"));

		// Report every compiled backend, available or not. GetUnavailableReason() is the whole
		// point: "Steam client not running" and "Steam is disabled in the editor process" are
		// completely different problems that both look like "no sessions found" from the UI.
		TArray<ECSOnlineBackend> Compiled;
		FCSOnlineBackendRegistry::Get().GetCompiledBackends(Compiled);
		for (ECSOnlineBackend Id : Compiled)
		{
			const TSharedPtr<ICSOnlineBackend> Backend = FCSOnlineBackendRegistry::Get().Find(Id);
			if (!Backend.IsValid())
			{
				UE_LOG(LogCS, Display, TEXT("  backend %-6s : <not registered>"), BackendName(Id));
				continue;
			}

			const bool bAvailable = Backend->IsAvailable();
			UE_LOG(LogCS, Display, TEXT("  backend %-6s : available=%s  oss=%s  lobbies=%s  netdriver=%s%s%s"),
				BackendName(Id),
				bAvailable ? TEXT("YES") : TEXT("no "),
				*Backend->GetSubsystemName().ToString(),
				Backend->UsesLobbies() ? TEXT("true") : TEXT("false"),
				*Backend->GetNetDriverClassName(),
				bAvailable ? TEXT("") : TEXT("  reason="),
				bAvailable ? TEXT("") : *Backend->GetUnavailableReason());
		}

		UCSOnlineSessionSubsystem* Online = GetOnline(World);
		if (!Online)
		{
			return;
		}

		UE_LOG(LogCS, Display, TEXT("  ACTIVE   : %s (%s)"),
			BackendName(Online->GetActiveBackend()),
			*Online->GetActiveBackendDisplayName().ToString());
		UE_LOG(LogCS, Display, TEXT("  identity : loggedIn=%s  nickname='%s'  netId='%s'"),
			Online->IsLoggedIn() ? TEXT("true") : TEXT("false"),
			*Online->GetLocalPlayerNickname(),
			*Online->GetLocalPlayerNetIdString());
		UE_LOG(LogCS, Display, TEXT("  session  : inSession=%s  busy=%s"),
			Online->IsInSession() ? TEXT("true") : TEXT("false"),
			Online->IsBusy() ? TEXT("true") : TEXT("false"));
		UE_LOG(LogCS, Display, TEXT("  transport: GameNetDriver=%s"),
			*FCSNetDriverSwitcher::GetCurrentGameNetDriverClassName());
	}

	static void CmdBackend(const TArray<FString>& Args, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogCS, Display, TEXT("usage: cs.Online.Backend <EIK|STEAM|NULL>"));
			return;
		}

		bool bOk = false;
		const ECSOnlineBackend Requested = UCSOnlineSettings::ParseBackendName(Args[0], bOk);
		if (!bOk)
		{
			UE_LOG(LogCS, Error, TEXT("[cs.Online] '%s' is not a backend name. Use EIK, STEAM or NULL."), *Args[0]);
			return;
		}

		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			FString Error;
			if (Online->SetActiveBackend(Requested, Error))
			{
				UE_LOG(LogCS, Display, TEXT("[cs.Online] active backend is now %s."), BackendName(Online->GetActiveBackend()));
				UE_LOG(LogCS, Warning,
					TEXT("[cs.Online] NOTE: this redirects only code routed through CSOnlineSessionSubsystem. ")
					TEXT("AdvancedSessions / OnlineSubsystemUtils Blueprint nodes still resolve through ")
					TEXT("[OnlineSubsystem] DefaultPlatformService. Use cs.Online.BackendRestart for a full switch."));
			}
			else
			{
				UE_LOG(LogCS, Error, TEXT("[cs.Online] switch to %s refused: %s"), BackendName(Requested), *Error);
			}
		}
	}

	static void CmdBackendRestart(const TArray<FString>& Args, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogCS, Display, TEXT("usage: cs.Online.BackendRestart <EIK|STEAM|NULL>"));
			return;
		}

		bool bOk = false;
		const ECSOnlineBackend Requested = UCSOnlineSettings::ParseBackendName(Args[0], bOk);
		if (!bOk)
		{
			UE_LOG(LogCS, Error, TEXT("[cs.Online] '%s' is not a backend name."), *Args[0]);
			return;
		}

		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			Online->RequestBackendSwitchWithRestart(Requested);
		}
	}

	static void CmdLogin(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			UE_LOG(LogCS, Display, TEXT("[cs.Online] Login() on %s..."), BackendName(Online->GetActiveBackend()));
			Online->Login();
		}
	}

	static void CmdHost(const TArray<FString>& Args, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			FCSHostSessionParams Params;
			if (Args.Num() >= 1)
			{
				const int32 Parsed = FCString::Atoi(*Args[0]);
				if (Parsed >= 1)
				{
					Params.MaxPlayers = Parsed;
				}
				else
				{
					UE_LOG(LogCS, Warning, TEXT("[cs.Online] MaxPlayers '%s' invalid; using %d."), *Args[0], Params.MaxPlayers);
				}
			}

			UE_LOG(LogCS, Display, TEXT("[cs.Online] HostSession(MaxPlayers=%d) on %s..."),
				Params.MaxPlayers, BackendName(Online->GetActiveBackend()));
			Online->HostSession(Params);
		}
	}

	static void CmdFind(const TArray<FString>& Args, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			FCSFindSessionsParams Params;
			if (Args.Num() >= 1)
			{
				const int32 Parsed = FCString::Atoi(*Args[0]);
				if (Parsed >= 1)
				{
					Params.MaxResults = Parsed;
				}
			}

			UE_LOG(LogCS, Display, TEXT("[cs.Online] FindSessions(MaxResults=%d, filterGameId=%s) on %s..."),
				Params.MaxResults,
				Params.bFilterByGameIdTag ? TEXT("true") : TEXT("false"),
				BackendName(Online->GetActiveBackend()));
			Online->FindSessions(Params);
		}
	}

	static void CmdList(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			TArray<FCSSessionSearchResult> Results;
			Online->GetLastSearchResults(Results);

			if (Results.Num() == 0)
			{
				UE_LOG(LogCS, Display,
					TEXT("[cs.Online] no cached results. Run cs.Online.Find first. If Find reported success ")
					TEXT("but nothing is listed, the GameId/BuildId filter dropped everything."));
				return;
			}

			UE_LOG(LogCS, Display, TEXT("=== %d session(s) ==="), Results.Num());
			for (const FCSSessionSearchResult& R : Results)
			{
				UE_LOG(LogCS, Display, TEXT("  [%2d] '%s' by '%s'  %d/%d players  ping=%dms  map='%s'  lan=%s  jip=%s"),
					R.ResultIndex,
					*R.SessionDisplayName,
					*R.OwningPlayerName,
					R.CurrentPlayers,
					R.MaxPlayers,
					R.PingInMs,
					*R.MapName,
					R.bIsLAN ? TEXT("y") : TEXT("n"),
					R.bAllowJoinInProgress ? TEXT("y") : TEXT("n"));
			}
			UE_LOG(LogCS, Display, TEXT("  join with: cs.Online.Join <index>"));
		}
	}

	static void CmdJoin(const TArray<FString>& Args, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogCS, Display, TEXT("usage: cs.Online.Join <index>   (see cs.Online.List)"));
			return;
		}

		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			const int32 Index = FCString::Atoi(*Args[0]);
			UE_LOG(LogCS, Display, TEXT("[cs.Online] JoinSessionByIndex(%d)..."), Index);
			Online->JoinSessionByIndex(Index);
		}
	}

	static void CmdLeave(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			UE_LOG(LogCS, Display, TEXT("[cs.Online] LeaveSession()..."));
			Online->LeaveSession();
		}
	}

	static void CmdCancelFind(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Ar*/)
	{
		if (UCSOnlineSessionSubsystem* Online = GetOnline(World))
		{
			Online->CancelFindSessions();
		}
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineStatusCmd(
	TEXT("cs.Online.Status"),
	TEXT("Print the active online backend, every compiled backend's availability, identity and net driver."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdStatus));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineBackendCmd(
	TEXT("cs.Online.Backend"),
	TEXT("cs.Online.Backend <EIK|STEAM|NULL> - switch the facade's backend in-process."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdBackend));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineBackendRestartCmd(
	TEXT("cs.Online.BackendRestart"),
	TEXT("cs.Online.BackendRestart <EIK|STEAM|NULL> - persist the backend choice and ask for a relaunch (also moves the AdvancedSessions Blueprint nodes)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdBackendRestart));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineLoginCmd(
	TEXT("cs.Online.Login"),
	TEXT("Log in on the active backend."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdLogin));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineHostCmd(
	TEXT("cs.Online.Host"),
	TEXT("cs.Online.Host [MaxPlayers] - create + start a session, then ServerTravel as listen host."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdHost));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineFindCmd(
	TEXT("cs.Online.Find"),
	TEXT("cs.Online.Find [MaxResults] - search for sessions on the active backend."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdFind));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineListCmd(
	TEXT("cs.Online.List"),
	TEXT("Print the cached search results with their join indices."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdList));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineJoinCmd(
	TEXT("cs.Online.Join"),
	TEXT("cs.Online.Join <index> - join a cached search result and ClientTravel to it."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdJoin));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineLeaveCmd(
	TEXT("cs.Online.Leave"),
	TEXT("Destroy / leave the current session."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdLeave));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCSOnlineCancelFindCmd(
	TEXT("cs.Online.CancelFind"),
	TEXT("Cancel an in-flight session search."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CSOnlineDebug::CmdCancelFind));

#endif // !UE_BUILD_SHIPPING
