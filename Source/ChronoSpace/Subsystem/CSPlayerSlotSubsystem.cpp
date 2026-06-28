// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSPlayerSlotSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/LocalPlayer.h"

ECSPlayerSlot UCSPlayerSlotSubsystem::EnsureSlotForController(APlayerController* PC)
{
    if (!PC)
    {
        return ECSPlayerSlot::Player0;
    }

    const FString Key = MakePlayerKey(PC);

    if (const ECSPlayerSlot* Existing = SlotByPlayerKey.Find(Key))
    {
        return *Existing;
    }

    const ECSPlayerSlot NewSlot = PickLowestFreeSlot();
    SlotByPlayerKey.Add(Key, NewSlot);

    UE_LOG(LogTemp, Log,
        TEXT("CSPlayerSlotSubsystem: assigned %s to '%s'"),
        NewSlot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"),
        *Key);

    return NewSlot;
}

void UCSPlayerSlotSubsystem::ReleaseSlotForController(APlayerController* PC)
{
    if (!PC) return;
    const FString Key = MakePlayerKey(PC);
    if (SlotByPlayerKey.Remove(Key) > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CSPlayerSlotSubsystem: released slot for '%s'"), *Key);
    }
}

void UCSPlayerSlotSubsystem::ResetAllSlots()
{
    SlotByPlayerKey.Empty();
    UE_LOG(LogTemp, Log, TEXT("CSPlayerSlotSubsystem: all slots reset"));
}

FString UCSPlayerSlotSubsystem::MakePlayerKey(APlayerController* PC)
{
    if (!PC) return FString();

    int32 LpId = 0;
    if (const ULocalPlayer* LP = PC->GetLocalPlayer())
    {
        LpId = LP->GetControllerId();
    }

    // 1) Real network (EOS): prefer UniqueNetId — persists across travel and
    //    across machines. We reach through to the inner FUniqueNetId's virtual
    //    ToString() because in some UE 5.5 + OnlineSubsystemEIK configs the
    //    wrapper's ToString fails to link (declared dllimport, not exported).
    //    LpId is appended so split-screen players sharing one NetId stay distinct.
    if (const APlayerState* PS = PC->PlayerState)
    {
        const FUniqueNetIdRepl& NetIdRepl = PS->GetUniqueId();
        if (NetIdRepl.IsValid())
        {
            if (const FUniqueNetId* RawId = NetIdRepl.GetUniqueNetId().Get())
            {
                const FString IdStr = RawId->ToString();
                if (!IdStr.IsEmpty() && IdStr != TEXT("INVALID"))
                {
                    return FString::Printf(TEXT("net:%s|%d"), *IdStr, LpId);
                }
            }
        }
    }

    // 2) PIE / local listen server fallback. Travel-stable because:
    //    - Server-side classification (local vs remote) is intrinsic to the
    //      connection role and persists across ServerTravel: the host's own
    //      PlayerController is always IsLocalController()==true on the server
    //      side; PCs spawned for net clients are always IsLocalController()==false.
    //    - ULocalPlayer (and its ControllerId) is owned by the GameInstance and
    //      survives ServerTravel, so split-screen players stay distinct.
    //    PC pointer is deliberately NOT used here — it changes on every
    //    non-seamless travel and would let stale entries occupy both slots,
    //    pushing every new joiner into the "both taken" fallback (Player1).
    const TCHAR* Side = PC->IsLocalController() ? TEXT("local") : TEXT("remote");
    return FString::Printf(TEXT("%s:%d"), Side, LpId);
}

ECSPlayerSlot UCSPlayerSlotSubsystem::PickLowestFreeSlot() const
{
    bool bP0Taken = false;
    bool bP1Taken = false;
    for (const TPair<FString, ECSPlayerSlot>& Pair : SlotByPlayerKey)
    {
        if (Pair.Value == ECSPlayerSlot::Player0) bP0Taken = true;
        else if (Pair.Value == ECSPlayerSlot::Player1) bP1Taken = true;
    }
    if (!bP0Taken) return ECSPlayerSlot::Player0;
    if (!bP1Taken) return ECSPlayerSlot::Player1;
    // Both already taken (more than 2 players in a 2-slot system): default to Player1.
    return ECSPlayerSlot::Player1;
}
