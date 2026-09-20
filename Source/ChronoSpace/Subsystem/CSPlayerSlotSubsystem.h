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
 * 슬롯 키는 서버가 보는 연결 신원 (local/remote, LocalPlayerControllerId) 이다.
 * UniqueNetId 는 쓰지 않는다 — 이유는 MakePlayerKey 주석 참고.
 * 요약하면 EOS DeviceID 로그인이 기기 단위라, 한 PC 에서 호스트와 클라를 같이 띄우면
 * 둘의 NetId 가 같아져 키가 겹치고 둘 다 Player0(토끼)이 된다.
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

    /**
     * 이 PC 의 슬롯을 지정한 값으로 덮어쓴다.
     * ACSGameMode 가 중복 슬롯을 발견해 반대쪽으로 돌렸을 때, 그 결정이 다음 트래블까지
     * 남도록 되돌려 넣는 용도다. 일반 경로는 EnsureSlotForController 를 쓴다.
     */
    void AssignSlotForController(APlayerController* PC, ECSPlayerSlot Slot);

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
