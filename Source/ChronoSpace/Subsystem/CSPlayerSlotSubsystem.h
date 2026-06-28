// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Player/CSPlayerState.h"
#include "CSPlayerSlotSubsystem.generated.h"

class APlayerController;

/**
 * Server-side authority on which ECSPlayerSlot each connected player owns.
 *
 * Why this exists:
 *   - ACSPlayerState::PlayerSlot used to be reseeded by join-order in PostLogin
 *     on every map. Non-seamless ServerTravel destroys/recreates the PlayerState
 *     and the EOS reconnect order is not the original join order, so players
 *     would silently swap or both default to Player0 (both become Rabbit).
 *   - GameInstance subsystems are the one piece of state that survives
 *     non-seamless ServerTravel on the listen server, so the assignment can
 *     live here and outlast individual maps.
 *
 * The slot is keyed by (UniqueNetId, LocalPlayerControllerId):
 *   - UniqueNetId is the EOS account id — stable across travel and re-join.
 *   - ControllerId disambiguates split-screen players that share one NetId.
 *
 * Server-only meaningful; clients receive the resolved slot via the
 * replicated PlayerSlot on ACSPlayerState.
 */
UCLASS()
class CHRONOSPACE_API UCSPlayerSlotSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Returns the slot for this PC, assigning the lowest-free slot on first contact. */
    ECSPlayerSlot EnsureSlotForController(APlayerController* PC);

    /** Drops the slot mapping for this PC (call from real Logout, not travel teardown). */
    void ReleaseSlotForController(APlayerController* PC);

    /** Clears all assignments (e.g. starting a new session / NewGame). */
    UFUNCTION(BlueprintCallable, Category = "PlayerSlot")
    void ResetAllSlots();

protected:
    // Persists across non-seamless ServerTravel because the GameInstance does.
    TMap<FString, ECSPlayerSlot> SlotByPlayerKey;

    static FString MakePlayerKey(APlayerController* PC);
    ECSPlayerSlot PickLowestFreeSlot() const;
};
