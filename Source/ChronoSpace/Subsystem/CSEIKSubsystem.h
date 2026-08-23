// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FindSessionsCallbackProxy.h"   // FBlueprintSessionResult (OnlineSubsystemUtils)
#include "CSEIKSubsystem.generated.h"

class UCSOnlineSessionSubsystem;
// Declared as `class` in OnlineSessionSettings.h:530. Forward-declaring it as `struct`
// is MSVC C4099, which this project promotes to an error.
class FOnlineSessionSearchResult;

/**
 * DEPRECATED forwarding shim.
 *
 * This class used to own the entire session flow, hardcoded to IOnlineSubsystem::Get(TEXT("EIK")).
 * That flow now lives in UCSOnlineSessionSubsystem, which is backend-agnostic and works on both
 * EIK and Steam. The class is kept only so that the four BlueprintCallable signatures it published
 * keep resolving if anything is ever wired to them; it holds no state of its own.
 *
 * New code must use UCSOnlineSessionSubsystem directly.
 *
 * Defects removed by rewriting rather than patching:
 *   - Initialize() had a `return;` BEFORE Super::Initialize(), so the subsystem never initialised.
 *   - JoinSessionForBlueprint() had an empty body; Blueprints calling it failed silently.
 *   - Nine hardcoded TEXT("EIK") literals.
 *   - OnLoginCompleteDelegates->AddUObject() added on every call and never cleared.
 *   - Four completion handlers dereferenced GetSessionInterface() with no null check.
 *   - ClientTravel ran BEFORE the join result was checked.
 *   - A fresh FGuid device id per launch, so the account was never stable.
 */
UCLASS(meta = (DeprecatedNode))
class CHRONOSPACE_API UCSEIKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Use CSOnlineSessionSubsystem::Login."))
	void LoginWithDeviceId();

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Use CSOnlineSessionSubsystem::HostSession."))
	void CreateSession();

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Use CSOnlineSessionSubsystem::FindSessions."))
	void FindSessions();

	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Online|Deprecated",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Use CSOnlineSessionSubsystem::JoinSessionByResult."))
	void JoinSessionForBlueprint(FBlueprintSessionResult& SearchResult);

	// FOnlineSessionSearchResult is not Blueprint-exposable; native callers use this.
	void JoinSession(const FOnlineSessionSearchResult& SearchResult);

private:
	UCSOnlineSessionSubsystem* GetOnline() const;
};
