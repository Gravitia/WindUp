// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "CSOnlineTypes.generated.h"

class UWorld;

/**
 * Blueprint-facing enums, structs and dynamic delegates for the ChronoSpace online layer.
 *
 * THIS FILE CONTAINS ZERO #if GUARDS, DELIBERATELY.
 * ECSOnlineBackend must list EIK and Steam unconditionally: Blueprint assets serialize
 * enum values BY NAME, so if an enumerator vanished when a backend is compiled out, every
 * Blueprint referencing it would break on the eventual backend cutover. This enum is data,
 * not backend code. Do not wrap anything here in CS_WITH_EIK / CS_WITH_STEAM.
 */

/** Which online subsystem the facade is talking to. Values are stable; never renumber. */
UENUM(BlueprintType)
enum class ECSOnlineBackend : uint8
{
	None	= 0		UMETA(DisplayName = "None (unresolved)"),
	EIK		= 1		UMETA(DisplayName = "EOS / EIK"),
	Steam	= 2		UMETA(DisplayName = "Steam"),
	Null	= 3		UMETA(DisplayName = "Null (LAN / offline)")
};

/** Result of any async online operation exposed by UCSOnlineSessionSubsystem. */
UENUM(BlueprintType)
enum class ECSOnlineOpResult : uint8
{
	Success = 0,
	Failed,
	Busy,
	NoBackend,
	NotLoggedIn,
	InvalidSession,
	AlreadyInSession,
	SessionFull,
	VersionMismatch,
	Cancelled,
	Timeout
};

/** Who can discover and join a hosted session. Mapped per-backend by the adapters. */
UENUM(BlueprintType)
enum class ECSSessionVisibility : uint8
{
	Public = 0,
	FriendsOnly,
	InviteOnly
};

/** This machine's role in the current session. */
UENUM(BlueprintType)
enum class ECSOnlineRole : uint8
{
	Client = 0,
	ListenHost,
	DedicatedServer
};

/** Everything the caller supplies to host a session. Backend-neutral by design. */
USTRUCT(BlueprintType)
struct CHRONOSPACE_API FCSHostSessionParams
{
	GENERATED_BODY()

	/**
	 * Must be >= 1. FindEOSSession unconditionally injects a NumPublicConnections >= 1
	 * filter (OnlineSessionEOS.cpp:2415), so a session with zero public connections is
	 * invisible on the EIK Sessions path.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxPlayers = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	ECSSessionVisibility Visibility = ECSSessionVisibility::Public;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	bool bAllowJoinInProgress = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	bool bIsLANMatch = false;

	/** Advertised under UCSOnlineSettings::SessionNameKey ("CSNAME"). Empty falls back to the local nickname. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FString SessionDisplayName;

	/** Null falls back to UCSOnlineSettings::DefaultHostLevel. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	TSoftObjectPtr<UWorld> TravelLevel;

	/**
	 * Appended verbatim to the travel URL, e.g. "?SpawnTag=Start".
	 * Must NOT contain "?listen" -- the online layer adds it. HostSession strips it
	 * defensively and logs a Warning: the travel URL is built by the layer, never typed
	 * at a call site, so that host travel cannot diverge between callers.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FString ExtraTravelOptions;

	/** Empty uses UCSOnlineSettings::GameIdTag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	FString GameIdTagOverride;
};

/** Everything the caller supplies to search for sessions. */
USTRUCT(BlueprintType)
struct CHRONOSPACE_API FCSFindSessionsParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session", meta = (ClampMin = "1"))
	int32 MaxResults = 50;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	bool bLANQuery = false;

	/**
	 * Leave true. On Steam appid 480 RequestLobbyList returns every Spacewar lobby on
	 * Steam; the advertised game-id tag is the only server-side discriminator available.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	bool bFilterByGameIdTag = true;

	/**
	 * Client-side BuildUniqueId check. Required because the EIK *lobby* path applies no
	 * bucket filter at all (only the Sessions path does, OnlineSessionEOS.cpp:2417).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Session")
	bool bFilterByBuildId = true;
};

/**
 * A read-only *view* of one search result. This is never the join token.
 *
 * WHY AN INDEX AND A GENERATION, NOT A COPIED FOnlineSessionSearchResult:
 * EIK's JoinLobbySession requires the lobby to still be present in LobbySearchResultsCache,
 * keyed by session-id string (OnlineSessionEOS.cpp:4068-4076), and that cache is wiped at
 * the start of every StartLobbySearch (:4526). A FOnlineSessionSearchResult copied out of a
 * superseded search therefore cannot be joined. SearchGeneration is compared against the
 * subsystem's counter on every join; a mismatch returns ECSOnlineOpResult::InvalidSession
 * with a clear log line instead of failing deep inside EOS.
 */
USTRUCT(BlueprintType)
struct CHRONOSPACE_API FCSSessionSearchResult
{
	GENERATED_BODY()

	/** Index into the live FOnlineSessionSearch owned by UCSOnlineSessionSubsystem. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 ResultIndex = INDEX_NONE;

	/** Which FindSessions pass produced this. Stale generations are refused on join. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 SearchGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	ECSOnlineBackend Backend = ECSOnlineBackend::None;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString SessionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString OwningPlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString SessionIdString;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 PingInMs = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 OpenSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsLAN = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bAllowJoinInProgress = false;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsFull = false;
};

// --- Blueprint-assignable completion delegates -------------------------------------
// Broadcast by UCSOnlineSessionSubsystem. The native OSS delegates never reach Blueprints;
// these dynamic multicast forms are the entire BP surface of the online layer.

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnLoginCompleteBP, bool, bWasSuccessful, const FString&, PlayerNickname, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnHostCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
// const TArray<>& as a dynamic-delegate parameter is legal -- the engine's own
// FBlueprintFindSessionsResultDelegate (FindSessionsCallbackProxy.h:19) does exactly this.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnFindSessionsCompleteBP, ECSOnlineOpResult, Result, const TArray<FCSSessionSearchResult>&, Results, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnJoinSessionCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnLeaveSessionCompleteBP, ECSOnlineOpResult, Result, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnBackendChangedBP, ECSOnlineBackend, OldBackend, ECSOnlineBackend, NewBackend);
