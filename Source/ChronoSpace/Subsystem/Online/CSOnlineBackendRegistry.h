// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Subsystem/Online/CSOnlineBackend.h"
#include "Subsystem/Online/CSOnlineTypes.h"

/**
 * Lazy singleton owning exactly one instance of every compiled-in adapter.
 *
 * THERE ARE NO #if GUARDS IN THIS HEADER, DELIBERATELY.
 * The guards live in CSOnlineBackendRegistry.cpp, which is the only translation unit in
 * the codebase that names both FCSOnlineBackend_EIK and FCSOnlineBackend_Steam. Nothing
 * else may include an adapter header, guarded or not. That is what makes the eventual
 * Steam-only cutover a file deletion (drop CSOnlineBackend_EIK.{h,cpp}, drop the plugin
 * from the .uproject, CS_WITH_EIK self-resolves to 0) rather than a diff across every
 * caller. If a #if for a specific backend ever appears outside the .cpp, the isolation
 * this whole layer exists to provide has been broken.
 *
 * WHY A LAZY SINGLETON AND NOT A MODULE STARTUP HOOK:
 * ChronoSpace.cpp:7 uses IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ...), so
 * there is no custom module class to hang a StartupModule on, and adding one purely for
 * this is more surface than the problem needs. A function-local static is lazy,
 * deterministic, thread-safe since C++11, and has no static-initialisation-order hazard.
 * First touch is UCSOnlineSessionSubsystem::Initialize.
 *
 * Plain C++: no UCLASS, no .generated.h. Adapters are held by TSharedRef and must be
 * constructible before any UObject exists.
 */
class CHRONOSPACE_API FCSOnlineBackendRegistry
{
public:
	static FCSOnlineBackendRegistry& Get();

	/** Returns an invalid TSharedPtr when that backend was compiled out of this build. */
	TSharedPtr<ICSOnlineBackend> Find(ECSOnlineBackend Id) const;

	/**
	 * Walks PreferenceOrder and returns the first adapter whose IsAvailable() is true,
	 * falling through to the Null adapter.
	 *
	 * Returns TSharedRef and not TSharedPtr deliberately: FCSOnlineBackend_Null is always
	 * compiled and always available, so this can never fail and UCSOnlineSessionSubsystem
	 * never has a null-backend code path to get wrong.
	 */
	TSharedRef<ICSOnlineBackend> FindFirstAvailable(TArrayView<const ECSOnlineBackend> PreferenceOrder) const;

	/** Every adapter present in this build, in registration order (real backends first, Null last). */
	void GetCompiledBackends(TArray<ECSOnlineBackend>& Out) const;

	/** The subset of GetCompiledBackends whose IsAvailable() is true right now. Calls into each adapter. */
	void GetAvailableBackends(TArray<ECSOnlineBackend>& Out) const;

	/**
	 * Reports the literal CS_WITH_* value for the requested backend, so UI can grey out a
	 * backend that was compiled out instead of offering it and failing at Initialize.
	 * Static because callers ask before the registry has any reason to exist.
	 */
	static bool IsBackendCompiledIn(ECSOnlineBackend Id);

	// Singleton. Copying or moving the registry would duplicate adapter instances that the
	// facade compares by pointer identity.
	FCSOnlineBackendRegistry(const FCSOnlineBackendRegistry&) = delete;
	FCSOnlineBackendRegistry& operator=(const FCSOnlineBackendRegistry&) = delete;
	FCSOnlineBackendRegistry(FCSOnlineBackendRegistry&&) = delete;
	FCSOnlineBackendRegistry& operator=(FCSOnlineBackendRegistry&&) = delete;

private:
	FCSOnlineBackendRegistry();

	void RegisterBackend(const TSharedRef<ICSOnlineBackend>& Backend);

	TMap<ECSOnlineBackend, TSharedRef<ICSOnlineBackend>> Backends;

	/** Registration order, so GetCompiledBackends is stable rather than TMap-hash ordered. */
	TArray<ECSOnlineBackend> RegistrationOrder;
};
