// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/Online/CSNetDriverSwitcher.h"

#include "Subsystem/Online/CSOnlineBackend.h"
#include "Settings/CSOnlineSettings.h"
#include "ChronoSpace.h"
#include "Engine/World.h"

// Process-lifetime storage, deliberately. GEngine outlives a PIE session, so the snapshot
// has to outlive the UCSOnlineSessionSubsystem that took it -- otherwise a Steam-backend
// test would leak its mutation into the next person's PIE run.
bool  FCSNetDriverSwitcher::bCaptured = false;
FName FCSNetDriverSwitcher::CapturedDriverClassName = NAME_None;
FName FCSNetDriverSwitcher::CapturedFallbackClassName = NAME_None;

namespace
{
	/**
	 * How many live net drivers could be holding a raw FNetDriverDefinition* into
	 * GEngine->NetDriverDefinitions right now.
	 *
	 * Returns INDEX_NONE when the answer cannot be determined (no GEngine, or a world with
	 * no world context). Every caller treats INDEX_NONE as "not safe" -- we refuse rather
	 * than guess, because guessing wrong is the dangling-pointer case.
	 */
	int32 CSCountActiveNetDrivers(const UWorld* World)
	{
		if (!GEngine)
		{
			return INDEX_NONE;
		}

		if (World)
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
			return Context ? Context->ActiveNetDrivers.Num() : INDEX_NONE;
		}

		// No world to scope the question to, so answer it for the whole engine: a driver
		// alive in ANY context holds a pointer into the one array we are about to write.
		int32 Total = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			Total += Context.ActiveNetDrivers.Num();
		}
		return Total;
	}
}

FNetDriverDefinition* FCSNetDriverSwitcher::FindGameNetDriverDefinition()
{
	if (!GEngine)
	{
		return nullptr;
	}

	// NAME_GameNetDriver is a hardcoded engine name (UnrealNames.inl:222,
	// REGISTER_NAME(282,GameNetDriver)), not a configurable string, so matching on it is
	// stable. There is exactly one such entry: NetDriverDefinitions allows only one
	// DriverClassName per DefName, which is the whole reason this class exists.
	return GEngine->NetDriverDefinitions.FindByPredicate(
		[](const FNetDriverDefinition& Definition)
		{
			return Definition.DefName == NAME_GameNetDriver;
		});
}

void FCSNetDriverSwitcher::CaptureDefaults()
{
	// Idempotent, and that is load-bearing. A second capture taken after we have already
	// mutated the entry would record OUR value as the "default", and RestoreDefaults would
	// then restore nothing. bCaptured is set once and never cleared -- not even by
	// RestoreDefaults -- for exactly this reason.
	if (bCaptured)
	{
		return;
	}

	const FNetDriverDefinition* Definition = FindGameNetDriverDefinition();
	if (!Definition)
	{
		// Not fatal here. ApplyClassName refuses later with the same diagnosis, and we must
		// not invent an entry to snapshot (see THE RULE in ApplyClassName).
		UE_LOG(LogCS, Warning,
			TEXT("[NetDriverSwitcher] CaptureDefaults: no DefName=\"GameNetDriver\" entry in GEngine->NetDriverDefinitions; nothing to snapshot."));
		return;
	}

	CapturedDriverClassName = Definition->DriverClassName;
	CapturedFallbackClassName = Definition->DriverClassNameFallback;
	bCaptured = true;

	UE_LOG(LogCS, Log, TEXT("[NetDriverSwitcher] Captured shipped GameNetDriver: Driver='%s', Fallback='%s'."),
		*CapturedDriverClassName.ToString(), *CapturedFallbackClassName.ToString());
}

bool FCSNetDriverSwitcher::ShouldManage(const UWorld* World)
{
	const UCSOnlineSettings& Settings = UCSOnlineSettings::Get();

	if (!Settings.bManageNetDriverDefinitions)
	{
		return false;
	}

	// The PIE guard is load-bearing, not a convenience.
	//
	// In the editor GEngine is UEditorEngine, which does not read
	// [/Script/Engine.GameEngine], so PIE's GameNetDriver comes from [/Script/Engine.Engine]
	// and is the plain IpNetDriver -- which is exactly what PIE wants. Forcing the EOS or
	// Steam driver there breaks PIE for no benefit; Steam OSS is force-disabled in-editor
	// regardless (OnlineSubsystemSteam.cpp:479-484).
	if (World && World->IsPlayInEditor() && !Settings.bManageNetDriverInPIE)
	{
		return false;
	}

	return true;
}

bool FCSNetDriverSwitcher::IsSafeToSwitch(const UWorld* World)
{
	return CSCountActiveNetDrivers(World) == 0;
}

bool FCSNetDriverSwitcher::ApplyForBackend(const ICSOnlineBackend& Backend, const UWorld* World, FString& OutError)
{
	OutError.Reset();

	const UCSOnlineSettings& Settings = UCSOnlineSettings::Get();
	const FString BackendName = Backend.GetDisplayName().ToString();

	// Policy gate. A management-disabled or PIE-skipped switch is NOT a failure: the caller
	// travels either way (UCSOnlineSessionSubsystem logs a Warning on false and proceeds),
	// so returning false here would print that Warning on every PIE host for no reason.
	// Returning true with OutError left empty means "nothing to do, and nothing is wrong".
	if (!ShouldManage(World))
	{
		UE_LOG(LogCS, Verbose,
			TEXT("[NetDriverSwitcher] Not managing GameNetDriver for backend '%s' (bManageNetDriverDefinitions=%d, PIE=%d, bManageNetDriverInPIE=%d). Leaving '%s' in place."),
			*BackendName,
			Settings.bManageNetDriverDefinitions ? 1 : 0,
			(World && World->IsPlayInEditor()) ? 1 : 0,
			Settings.bManageNetDriverInPIE ? 1 : 0,
			*GetCurrentGameNetDriverClassName());
		return true;
	}

	// Checked here so the message can name the backend, and checked AGAIN inside
	// ApplyClassName because that is the single mutation site and has to be safe for direct
	// callers (the cs.Online.* debug commands) too. Do not "clean up" the duplicate.
	if (!IsSafeToSwitch(World))
	{
		const int32 ActiveDrivers = CSCountActiveNetDrivers(World);
		OutError = (ActiveDrivers == INDEX_NONE)
			? FString::Printf(TEXT("Cannot switch to backend '%s': no world context, so active net drivers cannot be ruled out."), *BackendName)
			: FString::Printf(TEXT("Cannot switch to backend '%s': %d net driver(s) still active. Tear the session down before re-hosting."), *BackendName, ActiveDrivers);
		UE_LOG(LogCS, Warning, TEXT("[NetDriverSwitcher] %s"), *OutError);
		return false;
	}

	UE_LOG(LogCS, Log, TEXT("[NetDriverSwitcher] Applying net driver for backend '%s'."), *BackendName);

	return ApplyClassName(Backend.GetNetDriverClassName(), Settings.FallbackNetDriverClassName, World, OutError);
}

bool FCSNetDriverSwitcher::ApplyClassName(const FString& DriverClassName,
                                          const FString& FallbackClassName,
                                          const UWorld* World, FString& OutError)
{
	// =================================================================================
	// THE RULE -- read this before changing anything below.
	//
	// Mutate DriverClassName / DriverClassNameFallback IN PLACE only.
	// NEVER Add/Remove/Insert/Empty on GEngine->NetDriverDefinitions:
	// FNamedNetDriver holds a RAW FNetDriverDefinition* into that array
	// (Engine.h:312, stored at UnrealEngine.cpp:15045 --
	//  "new(Context.ActiveNetDrivers) FNamedNetDriver(ReturnVal, Definition)"),
	// so any reallocation dangles the definition pointer of every live net driver.
	//
	// Two consequences that look like missing features and are not:
	//   1. A missing GameNetDriver entry is an ERROR, not something we repair by adding
	//      one. Adding is the reallocating operation.
	//   2. This function refuses to run while any net driver is alive, even though the
	//      write itself is only two FName fields.
	// =================================================================================

	OutError.Reset();

	if (!GEngine)
	{
		OutError = TEXT("GEngine is null.");
		UE_LOG(LogCS, Error, TEXT("[NetDriverSwitcher] %s"), *OutError);
		return false;
	}

	if (DriverClassName.IsEmpty())
	{
		// Blanking DriverClassName would leave StaticLoadClass nothing to resolve and drop
		// the whole definition to its fallback silently. Refuse instead.
		OutError = TEXT("Empty DriverClassName; refusing to blank the GameNetDriver definition.");
		UE_LOG(LogCS, Error, TEXT("[NetDriverSwitcher] %s"), *OutError);
		return false;
	}

	// The safety gate, at the single mutation site. See the note in ApplyForBackend about
	// why this check exists in both places.
	const int32 ActiveDrivers = CSCountActiveNetDrivers(World);
	if (ActiveDrivers != 0)
	{
		OutError = (ActiveDrivers == INDEX_NONE)
			? FString(TEXT("No world context resolved, so active net drivers cannot be ruled out."))
			: FString::Printf(TEXT("%d net driver(s) still active; mutating the definition under a live driver is unsafe."), ActiveDrivers);
		UE_LOG(LogCS, Warning, TEXT("[NetDriverSwitcher] Refusing to switch: %s"), *OutError);
		return false;
	}

	FNetDriverDefinition* Definition = FindGameNetDriverDefinition();
	if (!Definition)
	{
		// Do NOT Add one. See THE RULE above.
		OutError = TEXT("No DefName=\"GameNetDriver\" entry in GEngine->NetDriverDefinitions. Check [/Script/Engine.GameEngine] in DefaultEngine.ini.");
		UE_LOG(LogCS, Error, TEXT("[NetDriverSwitcher] %s"), *OutError);
		return false;
	}

	const FName OldDriver = Definition->DriverClassName;
	const FName OldFallback = Definition->DriverClassNameFallback;

	// In place: two field writes on an element that already exists. The array itself, its
	// size and its allocation are never touched, so every FNamedNetDriver::NetDriverDef
	// stays valid.
	Definition->DriverClassName = FName(*DriverClassName);

	if (!FallbackClassName.IsEmpty())
	{
		Definition->DriverClassNameFallback = FName(*FallbackClassName);
	}
	else
	{
		// Keep whatever fallback is already configured rather than blanking it. The fallback
		// is the engine's free safety net: CreateNetDriver_Local falls back to
		// DriverClassNameFallback when the primary class fails to load OR its CDO reports
		// IsAvailable() == false (UnrealEngine.cpp:15020-15024). That is what turns a wrong
		// or unusable transport into IpNetDriver instead of a failed travel.
		UE_LOG(LogCS, Warning, TEXT("[NetDriverSwitcher] Empty fallback class name; keeping '%s'."),
			*OldFallback.ToString());
	}

	// This is the line to look for first when a travel fails. It is logged unconditionally,
	// including when nothing actually changed, because "the driver was already correct" is
	// itself the answer to half of those investigations.
	UE_LOG(LogCS, Log, TEXT("[NetDriverSwitcher] GameNetDriver: '%s' -> '%s' (fallback '%s' -> '%s')."),
		*OldDriver.ToString(), *Definition->DriverClassName.ToString(),
		*OldFallback.ToString(), *Definition->DriverClassNameFallback.ToString());

	// No StaticLoadClass probe and no IsAvailable() check here on purpose. The engine
	// already does both at driver-creation time (UnrealEngine.cpp:15020-15024), and both
	// USteamSocketsNetDriver::IsAvailable() and UNetDriverEIKBase report honestly, so a bad
	// setting degrades to IpNetDriver rather than crashing. Probing here would only add a
	// second, earlier, differently-behaving verdict.
	return true;
}

void FCSNetDriverSwitcher::RestoreDefaults()
{
	if (!bCaptured)
	{
		return;
	}

	FNetDriverDefinition* Definition = FindGameNetDriverDefinition();
	if (!Definition)
	{
		UE_LOG(LogCS, Warning,
			TEXT("[NetDriverSwitcher] RestoreDefaults: the GameNetDriver definition is gone; nothing restored."));
		return;
	}

	const FName OldDriver = Definition->DriverClassName;
	const FName OldFallback = Definition->DriverClassNameFallback;

	// In place, same rule as ApplyClassName.
	//
	// Deliberately NOT gated on IsSafeToSwitch. This runs from
	// UCSOnlineSessionSubsystem::Deinitialize, where refusing would be strictly worse:
	// GEngine outlives a PIE session, so an un-restored mutation leaks into the next PIE
	// run and into unrelated automation tests. And the write is safe even with drivers
	// alive -- the only post-creation read of FNamedNetDriver::NetDriverDef anywhere in the
	// engine is DefName (UnrealEngine.cpp:14852), which we never touch. The danger was only
	// ever reallocation, and this is not one.
	Definition->DriverClassName = CapturedDriverClassName;
	Definition->DriverClassNameFallback = CapturedFallbackClassName;

	// bCaptured stays true for the life of the process. Clearing it would let a later
	// CaptureDefaults re-snapshot, and if a restore had ever failed that snapshot would be
	// our own mutation masquerading as the shipped default.

	if (OldDriver != CapturedDriverClassName || OldFallback != CapturedFallbackClassName)
	{
		UE_LOG(LogCS, Log, TEXT("[NetDriverSwitcher] Restored GameNetDriver: '%s' -> '%s' (fallback '%s' -> '%s')."),
			*OldDriver.ToString(), *CapturedDriverClassName.ToString(),
			*OldFallback.ToString(), *CapturedFallbackClassName.ToString());
	}
}

FString FCSNetDriverSwitcher::GetCurrentGameNetDriverClassName()
{
	const FNetDriverDefinition* Definition = FindGameNetDriverDefinition();
	return Definition ? Definition->DriverClassName.ToString() : FString();
}
