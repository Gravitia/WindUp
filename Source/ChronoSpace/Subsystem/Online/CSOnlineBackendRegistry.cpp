// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/Online/CSOnlineBackendRegistry.h"

#include "Subsystem/Online/CSOnlineBackend_Null.h"
#include "Settings/CSOnlineSettings.h"
#include "ChronoSpace.h"

// CS_WITH_EIK / CS_WITH_STEAM / CS_WITH_STEAMSOCKETS are emitted as paired 1/0
// PublicDefinitions by ChronoSpace.Build.cs.
//
// This translation unit reads them BY VALUE in the "Registry built" log line below, not
// only inside #if, so an undefined macro has to fail loudly right here. Inside #if an
// undefined identifier quietly evaluates to 0: every real adapter would be compiled out,
// the game would sit permanently on the Null backend, and nothing in the log would say
// why. A build error naming the missing defines is the cheap failure mode; a silently
// backend-less packaged build is the expensive one.
#if !defined(CS_WITH_EIK) || !defined(CS_WITH_STEAM) || !defined(CS_WITH_STEAMSOCKETS)
	#error "CS_WITH_EIK / CS_WITH_STEAM / CS_WITH_STEAMSOCKETS must be emitted as paired 1/0 PublicDefinitions by ChronoSpace.Build.cs. See Docs/DualOSS_Design_Spec.md, section 2."
#endif

// THE ONLY GUARDED ADAPTER INCLUDES IN THE CODEBASE.
//
// No other translation unit may include an adapter header, guarded or not. That is what
// keeps the eventual single-backend cutover a file deletion -- drop
// CSOnlineBackend_EIK.{h,cpp}, drop the plugin from the .uproject, CS_WITH_EIK
// self-resolves to 0 -- instead of a diff across every caller. If a #if naming a specific
// backend ever appears outside this file, the isolation this layer exists to provide has
// already been broken.
//
// These are POLICY guards, not link guards: IOnlineSubsystem::Get(FName) resolves the DLL
// by string concatenation "OnlineSubsystem" + Name through FModuleManager at runtime
// (OnlineSubsystemModule.cpp:22-42, :382), so nothing here links against EIK or Steam.
#if CS_WITH_EIK
#include "Subsystem/Online/CSOnlineBackend_EIK.h"
#endif

#if CS_WITH_STEAM
#include "Subsystem/Online/CSOnlineBackend_Steam.h"
#endif

FCSOnlineBackendRegistry& FCSOnlineBackendRegistry::Get()
{
	// C++11 magic static: lazy, deterministic, thread-safe, no static-initialisation-order
	// hazard. ChronoSpace.cpp:7 uses IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ...),
	// so there is no custom module class to hang a StartupModule on, and adding one purely
	// for this would be more surface than the problem needs. First touch is
	// UCSOnlineSessionSubsystem::Initialize.
	static FCSOnlineBackendRegistry Instance;
	return Instance;
}

FCSOnlineBackendRegistry::FCSOnlineBackendRegistry()
{
	// Real backends first, Null last. GetCompiledBackends reports this order verbatim, and
	// the Null adapter is the terminator of the fallback chain rather than a peer of the
	// real backends.
#if CS_WITH_EIK
	RegisterBackend(MakeShared<FCSOnlineBackend_EIK>());
#endif

#if CS_WITH_STEAM
	RegisterBackend(MakeShared<FCSOnlineBackend_Steam>());
#endif

	// Unguarded, always present, always last. This registration is the whole reason
	// FindFirstAvailable can return a TSharedRef, which is in turn the reason
	// UCSOnlineSessionSubsystem never has a null-backend code path to get wrong.
	RegisterBackend(MakeShared<FCSOnlineBackend_Null>());

	UE_LOG(LogCS, Log, TEXT("[Online] Registry built: CS_WITH_EIK=%d CS_WITH_STEAM=%d CS_WITH_STEAMSOCKETS=%d, %d adapter(s)"),
		CS_WITH_EIK, CS_WITH_STEAM, CS_WITH_STEAMSOCKETS, Backends.Num());

	// A build with both real backends compiled out still runs -- offline / LAN only, on
	// Null. Say so once, up front, rather than letting someone discover it when hosting
	// silently produces a session nobody on the internet can see.
	if (Backends.Num() <= 1)
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Registry: no online backend is compiled into this build. Only %s (offline / LAN) is available. Check the EOSIntegrationKit / OnlineSubsystemSteam entries in ChronoSpace.uproject."),
			UCSOnlineSettings::LexToString(ECSOnlineBackend::Null));
	}
}

void FCSOnlineBackendRegistry::RegisterBackend(const TSharedRef<ICSOnlineBackend>& Backend)
{
	// GetBackendId() is deliberately the only adapter accessor called from the constructor
	// path. GetSubsystemName() and GetNetDriverClassName() read the UCSOnlineSettings CDO,
	// and this constructor must stay runnable without any UObject existing yet -- that
	// property is what allows the registry to be a plain function-local static at all.
	const ECSOnlineBackend Id = Backend->GetBackendId();

	if (Backends.Contains(Id))
	{
		// One instance per backend id. UCSOnlineSessionSubsystem compares adapters by
		// pointer identity, so a second instance of the same backend would compare unequal
		// to the active one and quietly break backend-switch bookkeeping.
		UE_LOG(LogCS, Error, TEXT("[Online] Registry: duplicate registration for backend %s ignored."),
			UCSOnlineSettings::LexToString(Id));
		return;
	}

	Backends.Add(Id, Backend);
	RegistrationOrder.Add(Id);
}

TSharedPtr<ICSOnlineBackend> FCSOnlineBackendRegistry::Find(ECSOnlineBackend Id) const
{
	if (const TSharedRef<ICSOnlineBackend>* Found = Backends.Find(Id))
	{
		return *Found;
	}

	// Not registered. An invalid TSharedPtr is returned in every case below -- a backend
	// that is compiled out never resolves to Null behind the back of the caller, because a
	// substituted backend is exactly the kind of silent success that turns "my friend
	// cannot see my session" into a multi-hour diagnosis.
	if (Id == ECSOnlineBackend::None)
	{
		// The unresolved sentinel, not a backend. Never registered, by design.
		UE_LOG(LogCS, Verbose, TEXT("[Online] Registry: ECSOnlineBackend::None has no adapter."));
	}
	else if (!IsBackendCompiledIn(Id))
	{
		UE_LOG(LogCS, Warning, TEXT("[Online] Registry: backend %s was requested but is not compiled into this build."),
			UCSOnlineSettings::LexToString(Id));
	}
	else
	{
		// Compiled in but absent from the map: a registration bug in this file, not a
		// configuration problem.
		UE_LOG(LogCS, Error, TEXT("[Online] Registry: backend %s is compiled in but was never registered."),
			UCSOnlineSettings::LexToString(Id));
	}

	return nullptr;
}

TSharedRef<ICSOnlineBackend> FCSOnlineBackendRegistry::FindFirstAvailable(TArrayView<const ECSOnlineBackend> PreferenceOrder) const
{
	for (int32 Index = 0; Index < PreferenceOrder.Num(); ++Index)
	{
		const ECSOnlineBackend Candidate = PreferenceOrder[Index];

		// Reads the map directly rather than calling Find(): Find() logs for a single
		// explicit lookup, and repeating that per candidate would bury the one line that
		// matters -- the fallback Warning below -- under one message per rejected entry.
		const TSharedRef<ICSOnlineBackend>* Entry = Backends.Find(Candidate);
		if (Entry == nullptr)
		{
			UE_LOG(LogCS, Verbose, TEXT("[Online] Registry: candidate %d/%d %s skipped (%s)."),
				Index + 1, PreferenceOrder.Num(),
				UCSOnlineSettings::LexToString(Candidate),
				IsBackendCompiledIn(Candidate) ? TEXT("not registered") : TEXT("compiled out of this build"));
			continue;
		}

		const TSharedRef<ICSOnlineBackend>& Backend = *Entry;

		// IsAvailable() is required to be cheap and safe to call at any time, including
		// before login, which is what makes walking the whole preference order here fine.
		if (Backend->IsAvailable())
		{
			if (Index > 0)
			{
				UE_LOG(LogCS, Warning, TEXT("[Online] Registry: preferred backend %s is not usable; using %s instead."),
					UCSOnlineSettings::LexToString(PreferenceOrder[0]),
					UCSOnlineSettings::LexToString(Backend->GetBackendId()));
			}

			return Backend;
		}

		// GetUnavailableReason() exists for this line and essentially only this line. Both
		// real backends fail silently by default: LoadDefaultSubsystem falls back to NULL
		// and logs the real cause only at LogOnline Verbose
		// (OnlineSubsystemModule.cpp:216-221), and FOnlineSubsystemSteam::Init() returns
		// false with nothing but "Could not set up the steam environment!".
		UE_LOG(LogCS, Verbose, TEXT("[Online] Registry: candidate %d/%d %s unavailable -- %s"),
			Index + 1, PreferenceOrder.Num(),
			UCSOnlineSettings::LexToString(Candidate),
			*Backend->GetUnavailableReason());
	}

	const TSharedRef<ICSOnlineBackend>* NullEntry = Backends.Find(ECSOnlineBackend::Null);

	// FCSOnlineBackend_Null carries no #if guard and the constructor registers it
	// unconditionally, so the only way to reach this is to have put that registration
	// behind a guard. There is no other terminator: this function returns TSharedRef.
	checkf(NullEntry != nullptr, TEXT("[Online] FCSOnlineBackend_Null must always be registered -- FindFirstAvailable returns TSharedRef and has no null path."));

	// Never fall through to Null quietly. Null hosts LAN-only sessions, so a player who
	// asked for EIK or Steam and silently got Null sees a session that simply nobody joins.
	UE_LOG(LogCS, Warning, TEXT("[Online] Registry: none of the %d preferred backend(s) was available; falling back to %s (offline / LAN)."),
		PreferenceOrder.Num(), UCSOnlineSettings::LexToString(ECSOnlineBackend::Null));

	return *NullEntry;
}

void FCSOnlineBackendRegistry::GetCompiledBackends(TArray<ECSOnlineBackend>& Out) const
{
	// Registration order, not TMap iteration order, so UI ordering is stable across runs.
	Out.Reset(RegistrationOrder.Num());
	Out.Append(RegistrationOrder);
}

void FCSOnlineBackendRegistry::GetAvailableBackends(TArray<ECSOnlineBackend>& Out) const
{
	Out.Reset(RegistrationOrder.Num());

	// A live query, not a cached list. Each IsAvailable() re-resolves
	// IOnlineSubsystem::Get(Name), which is what makes the answer change when the Steam
	// client is started or closed mid-run; do not memoise the result of this call.
	for (const ECSOnlineBackend Id : RegistrationOrder)
	{
		const TSharedRef<ICSOnlineBackend>* Entry = Backends.Find(Id);
		if (Entry != nullptr && (*Entry)->IsAvailable())
		{
			Out.Add(Id);
		}
	}
}

bool FCSOnlineBackendRegistry::IsBackendCompiledIn(ECSOnlineBackend Id)
{
	// Reports the literal CS_WITH_* value rather than consulting the registry, so UI can
	// grey a backend out before the registry has any reason to exist. A backend that is
	// compiled out answers false here and gets an invalid TSharedPtr from Find(); it never
	// resolves to some other backend.
	switch (Id)
	{
	case ECSOnlineBackend::EIK:
		return CS_WITH_EIK != 0;

	case ECSOnlineBackend::Steam:
		return CS_WITH_STEAM != 0;

	case ECSOnlineBackend::Null:
		// FCSOnlineBackend_Null has no #if guard, deliberately.
		return true;

	case ECSOnlineBackend::None:
		// The unresolved sentinel is not a backend and has no adapter.
		return false;

	default:
		// ECSOnlineBackend values are stable and never renumbered, so reaching this means a
		// new enumerator was added to CSOnlineTypes.h without an adapter and without a case
		// here. Answer false rather than guessing.
		return false;
	}
}
