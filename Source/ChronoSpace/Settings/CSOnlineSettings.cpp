// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/CSOnlineSettings.h"

#include "ChronoSpace.h"
#include "CoreGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

/**
 * A NAMED namespace, not an anonymous one, on purpose: UBT unity builds concatenate .cpp
 * files into a single translation unit, so two file-scope anonymous namespaces holding
 * identically named constants would collide in a non-obvious way.
 */
namespace CSOnlineSettingsPrivate
{
	/**
	 * PreferredBackend lives in GGameUserSettingsIni, NOT in DefaultEngine.ini where the rest
	 * of this class is persisted. It cannot be a Config UPROPERTY here: UCLASS(Config = Engine)
	 * would bind it to DefaultEngine.ini and the runtime override written by the dev menu
	 * would silently never be read back.
	 *
	 * Section and key are spelled once, here, and shared by ResolveStartupBackend() and
	 * WritePreferredBackend(). A mismatch between the two would fail silently -- GConfig's
	 * GetString just returns false for a missing key and the override is skipped, no error.
	 */
	constexpr TCHAR PreferredBackendSection[] = TEXT("/Script/ChronoSpace.CSOnlineSettings");
	constexpr TCHAR PreferredBackendKey[] = TEXT("PreferredBackend");

	/** The documented switch: -CSOnlineBackend=EIK|STEAM|NULL */
	constexpr TCHAR BackendCommandLineSwitch[] = TEXT("CSOnlineBackend=");
}

UCSOnlineSettings::UCSOnlineSettings()
{
	// Seed the walk order ONLY when it is still empty, never append.
	//
	// Config array properties are applied to the CDO after this constructor runs, and a
	// ReloadConfig() or a CDO reconstruct runs the constructor again over an object that may
	// already carry ini-supplied entries. Appending there would grow the list with duplicates
	// and quietly change the fallback order the subsystem walks.
	if (BackendFallbackOrder.Num() == 0)
	{
		BackendFallbackOrder.Add(ECSOnlineBackend::EIK);
		BackendFallbackOrder.Add(ECSOnlineBackend::Steam);
		BackendFallbackOrder.Add(ECSOnlineBackend::Null);
	}
}

const UCSOnlineSettings& UCSOnlineSettings::Get()
{
	// The CDO is the live edited settings object for a UDeveloperSettings, so this is the
	// same instance Project Settings writes to.
	const UCSOnlineSettings* Settings = GetDefault<UCSOnlineSettings>();
	check(Settings);
	return *Settings;
}

ECSOnlineBackend UCSOnlineSettings::ResolveStartupBackend() const
{
	// This returns an INTENT, not an available backend. Availability is resolved separately by
	// UCSOnlineSessionSubsystem::ResolveBackend, which walks BackendFallbackOrder. Keeping the
	// two apart is what lets the log say "requested Steam, Steam unavailable (client not
	// running), fell back to EIK" instead of just reporting EIK and hiding the real request.
	//
	// Every branch below logs the SOURCE it resolved from, so a mis-selection is one grep away.

	// 1. Command line. Highest priority so a QA or CI process can be pinned to a backend
	//    without writing to any ini file.
	{
		FString CommandLineValue;
		if (FParse::Value(FCommandLine::Get(), CSOnlineSettingsPrivate::BackendCommandLineSwitch, CommandLineValue))
		{
			bool bParsed = false;
			const ECSOnlineBackend Requested = ParseBackendName(CommandLineValue, bParsed);
			if (bParsed)
			{
				UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] Startup backend %s (source: command line -CSOnlineBackend=%s)."),
					LexToString(Requested), *CommandLineValue);
				return Requested;
			}

			// Warn and fall through rather than ignore silently. Someone who typed the switch
			// expects it to win; without this line the next source down picks a different
			// backend and nothing in the log explains why.
			UE_LOG(LogCS, Warning, TEXT("[CSOnlineSettings] -CSOnlineBackend=%s is not a known backend name (expected EIK/EOS, STEAM, or NULL/NONE/OFFLINE). Ignoring it and continuing down the precedence chain."),
				*CommandLineValue);
		}
	}

	// 2. PreferredBackend in GGameUserSettingsIni, written by WritePreferredBackend() from the
	//    in-game dev menu. Survives a restart.
	//    GConfig is null very early in startup and again after config teardown, so it is
	//    checked rather than assumed.
	if (GConfig)
	{
		FString PreferredValue;
		if (GConfig->GetString(CSOnlineSettingsPrivate::PreferredBackendSection, CSOnlineSettingsPrivate::PreferredBackendKey, PreferredValue, GGameUserSettingsIni))
		{
			bool bParsed = false;
			const ECSOnlineBackend Preferred = ParseBackendName(PreferredValue, bParsed);
			if (bParsed)
			{
				UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] Startup backend %s (source: GameUserSettings.ini [%s] %s=%s)."),
					LexToString(Preferred), CSOnlineSettingsPrivate::PreferredBackendSection, CSOnlineSettingsPrivate::PreferredBackendKey, *PreferredValue);
				return Preferred;
			}

			UE_LOG(LogCS, Warning, TEXT("[CSOnlineSettings] GameUserSettings.ini [%s] %s=%s is not a known backend name. Ignoring it and continuing down the precedence chain."),
				CSOnlineSettingsPrivate::PreferredBackendSection, CSOnlineSettingsPrivate::PreferredBackendKey, *PreferredValue);
		}
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSOnlineSettings] GConfig is unavailable; the PreferredBackend override could not be read."));
	}

	// 3. DefaultBackend from DefaultEngine.ini. None means "unresolved", never a real choice.
	//    Same [/Script/ChronoSpace.CSOnlineSettings] section path as step 2 -- UCLASS(Config =
	//    Engine) keys off the class path too -- but in DefaultEngine.ini, not GameUserSettings.
	if (DefaultBackend != ECSOnlineBackend::None)
	{
		UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] Startup backend %s (source: DefaultEngine.ini [%s] DefaultBackend)."),
			LexToString(DefaultBackend), CSOnlineSettingsPrivate::PreferredBackendSection);
		return DefaultBackend;
	}

	// 4. Head of the fallback order. Skipped when it is None for the same reason as above:
	//    returning the unresolved sentinel as an intent would push the "no backend" decision
	//    into the subsystem instead of degrading to the offline backend here.
	if (BackendFallbackOrder.Num() > 0 && BackendFallbackOrder[0] != ECSOnlineBackend::None)
	{
		UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] Startup backend %s (source: BackendFallbackOrder[0]; DefaultBackend is None)."),
			LexToString(BackendFallbackOrder[0]));
		return BackendFallbackOrder[0];
	}

	// 5. Final fallback. Null always exists and always works, so the layer degrades to
	//    LAN/offline instead of coming up with no backend at all.
	UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] Startup backend NULL (source: final fallback; no command line override, no PreferredBackend, DefaultBackend is None and BackendFallbackOrder has no usable head)."));
	return ECSOnlineBackend::Null;
}

ECSOnlineBackend UCSOnlineSettings::ParseBackendName(const FString& In, bool& bOutOk)
{
	bOutOk = false;

	// Trimmed because this value arrives from a command line or a hand-edited ini, both of
	// which pick up stray whitespace easily.
	const FString Token = In.TrimStartAndEnd();
	if (Token.IsEmpty())
	{
		return ECSOnlineBackend::None;
	}

	// EOS is accepted as an alias because the plugin is EOS Integration Kit and most people
	// say "EOS"; the subsystem itself is registered under the bare literal "EIK"
	// (OnlineSubsystemModuleEIK.cpp:66).
	if (Token.Equals(TEXT("EIK"), ESearchCase::IgnoreCase) ||
		Token.Equals(TEXT("EOS"), ESearchCase::IgnoreCase))
	{
		bOutOk = true;
		return ECSOnlineBackend::EIK;
	}

	if (Token.Equals(TEXT("STEAM"), ESearchCase::IgnoreCase))
	{
		bOutOk = true;
		return ECSOnlineBackend::Steam;
	}

	// NONE and OFFLINE both mean ECSOnlineBackend::Null -- the LAN/offline backend -- and
	// deliberately NOT ECSOnlineBackend::None, which is the "unresolved" sentinel and is never
	// a legal request. The asymmetry with LexToString(None) == "NONE" is intended: a human
	// typing NONE means "play offline", not "leave the backend unresolved".
	if (Token.Equals(TEXT("NULL"), ESearchCase::IgnoreCase) ||
		Token.Equals(TEXT("NONE"), ESearchCase::IgnoreCase) ||
		Token.Equals(TEXT("OFFLINE"), ESearchCase::IgnoreCase))
	{
		bOutOk = true;
		return ECSOnlineBackend::Null;
	}

	return ECSOnlineBackend::None;
}

const TCHAR* UCSOnlineSettings::LexToString(ECSOnlineBackend Backend)
{
	switch (Backend)
	{
	case ECSOnlineBackend::EIK:		return TEXT("EIK");
	case ECSOnlineBackend::Steam:	return TEXT("STEAM");
	case ECSOnlineBackend::Null:	return TEXT("NULL");
	case ECSOnlineBackend::None:	return TEXT("NONE");
	}

	// Reached only when an ini or a serialised asset carries a value outside the enum. There is
	// deliberately no default label above: with every enumerator listed explicitly, a new one
	// added to ECSOnlineBackend shows up as an unhandled case rather than being absorbed by a
	// catch-all and silently logging as "NONE".
	return TEXT("NONE");
}

void UCSOnlineSettings::WritePreferredBackend(ECSOnlineBackend Backend) const
{
	if (!GConfig)
	{
		UE_LOG(LogCS, Warning, TEXT("[CSOnlineSettings] WritePreferredBackend(%s) skipped: GConfig is unavailable."),
			LexToString(Backend));
		return;
	}

	if (Backend == ECSOnlineBackend::None)
	{
		// "NONE" parses back as ECSOnlineBackend::Null (offline), not as the unresolved
		// sentinel -- see ParseBackendName. Persisting the sentinel is therefore never what a
		// caller means, so say so rather than let the asymmetry surprise someone next launch.
		UE_LOG(LogCS, Warning, TEXT("[CSOnlineSettings] WritePreferredBackend was given ECSOnlineBackend::None. It is stored as \"NONE\" and will be read back as NULL (offline) on the next launch."));
	}

	const TCHAR* Token = LexToString(Backend);

	GConfig->SetString(CSOnlineSettingsPrivate::PreferredBackendSection, CSOnlineSettingsPrivate::PreferredBackendKey, Token, GGameUserSettingsIni);

	// Flush(bRemoveFromCache = false, Filename) -- the same GConfig+Flush idiom as
	// UCSLoginIdLibrary::GetPersistentDeviceLoginId (CSLoginIdLibrary.cpp:22-27). Without the
	// explicit flush the value only reaches disk at some later, unpredictable write, so a
	// crash or a hard exit loses the override and the next launch silently resolves a
	// different backend.
	GConfig->Flush(false, GGameUserSettingsIni);

	UE_LOG(LogCS, Log, TEXT("[CSOnlineSettings] PreferredBackend=%s written to GameUserSettings.ini [%s]."),
		Token, CSOnlineSettingsPrivate::PreferredBackendSection);
}
