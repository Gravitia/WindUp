// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"

class ICSOnlineBackend;
class UWorld;

/**
 * Retargets the GameNetDriver definition at runtime, immediately before ServerTravel /
 * ClientTravel.
 *
 * NetDriverDefinitions allows exactly one DriverClassName per DefName="GameNetDriver", so
 * the transport cannot be chosen per-call from ini. This helper mutates the existing
 * element IN PLACE instead.
 *
 * IN PLACE IS MANDATORY, NOT STYLISTIC. FNamedNetDriver::NetDriverDef is a raw
 * FNetDriverDefinition* pointing into GEngine->NetDriverDefinitions (Engine.h:312, stored
 * at UnrealEngine.cpp:15045), so any Add / Remove / Insert / Empty on that array can
 * realloc and dangle the definition pointer of every live net driver. ApplyClassName
 * therefore only ever writes DriverClassName / DriverClassNameFallback on an entry that
 * already exists, and refuses to run at all while the world context still has active net
 * drivers.
 *
 * PURE STRINGS: no #if, no backend headers, no knowledge of which backend is which. Class
 * paths come from UCSOnlineSettings via ICSOnlineBackend::GetNetDriverClassName().
 */
class CHRONOSPACE_API FCSNetDriverSwitcher
{
public:
	/**
	 * Snapshots the shipped GameNetDriver entry so RestoreDefaults can undo us.
	 * Idempotent -- the snapshot is taken once and never overwritten by a later call, or
	 * a second capture would record our own mutation as the "default".
	 * Called from UCSOnlineSessionSubsystem::Initialize.
	 */
	static void CaptureDefaults();

	/** ShouldManage -> IsSafeToSwitch -> ApplyClassName(Backend.GetNetDriverClassName(), ...). */
	static bool ApplyForBackend(const ICSOnlineBackend& Backend, const UWorld* World, FString& OutError);

	/**
	 * Writes DriverClassName / DriverClassNameFallback onto the existing GameNetDriver
	 * definition. Returns false with OutError set if no such definition exists -- it does
	 * NOT add one.
	 */
	static bool ApplyClassName(const FString& DriverClassName,
	                           const FString& FallbackClassName,
	                           const UWorld* World, FString& OutError);

	/**
	 * Writes the captured names back if CaptureDefaults ever ran.
	 *
	 * MUST be called from UCSOnlineSessionSubsystem::Deinitialize(). GEngine outlives a
	 * PIE session, so an un-restored mutation silently leaks into the next PIE run and
	 * into unrelated automation tests.
	 */
	static void RestoreDefaults();

	/**
	 * True only when GEngine->GetWorldContextFromWorld(World) reports
	 * ActiveNetDrivers.Num() == 0. In the happy path the host is in the title map at host
	 * time and the client likewise at join time, so this never fires; it exists to catch a
	 * re-host without a proper teardown, where mutating the definition under a live driver
	 * would be the dangling-pointer case described above.
	 */
	static bool IsSafeToSwitch(const UWorld* World);

	/**
	 * UCSOnlineSettings::bManageNetDriverDefinitions, and false in PIE unless
	 * bManageNetDriverInPIE.
	 *
	 * The PIE guard is load-bearing. In the editor GEngine is UEditorEngine, which does
	 * not read [/Script/Engine.GameEngine], so PIE's GameNetDriver comes from
	 * [/Script/Engine.Engine] and is the plain IpNetDriver -- which is what PIE wants.
	 * Forcing EOS or Steam drivers there breaks PIE for no benefit; Steam is disabled
	 * in-editor regardless (OnlineSubsystemSteam.cpp:479-484).
	 */
	static bool ShouldManage(const UWorld* World);

	/** Current DriverClassName on the GameNetDriver definition; empty when there is none. For logging and debug commands. */
	static FString GetCurrentGameNetDriverClassName();

private:
	/** Looks up the DefName == NAME_GameNetDriver entry. NAME_GameNetDriver is a hardcoded engine name (UnrealNames.inl:222). */
	static FNetDriverDefinition* FindGameNetDriverDefinition();

	static bool  bCaptured;
	static FName CapturedDriverClassName;
	static FName CapturedFallbackClassName;
};
