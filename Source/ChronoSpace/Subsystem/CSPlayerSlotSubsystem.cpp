// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSPlayerSlotSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/LocalPlayer.h"
#include "ChronoSpace.h"

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

    UE_LOG(LogCS, Log,
        TEXT("CSPlayerSlotSubsystem: assigned %s to '%s'"),
        NewSlot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"),
        *Key);

    return NewSlot;
}

void UCSPlayerSlotSubsystem::AssignSlotForController(APlayerController* PC, ECSPlayerSlot Slot)
{
    if (!PC) return;

    const FString Key = MakePlayerKey(PC);
    SlotByPlayerKey.Add(Key, Slot);

    UE_LOG(LogCS, Log,
        TEXT("CSPlayerSlotSubsystem: forced %s onto '%s'"),
        Slot == ECSPlayerSlot::Player0 ? TEXT("Player0") : TEXT("Player1"),
        *Key);
}

void UCSPlayerSlotSubsystem::ReleaseSlotForController(APlayerController* PC)
{
    if (!PC) return;
    const FString Key = MakePlayerKey(PC);
    if (SlotByPlayerKey.Remove(Key) > 0)
    {
        UE_LOG(LogCS, Log, TEXT("CSPlayerSlotSubsystem: released slot for '%s'"), *Key);
    }
}

void UCSPlayerSlotSubsystem::ResetAllSlots()
{
    SlotByPlayerKey.Empty();
    UE_LOG(LogCS, Log, TEXT("CSPlayerSlotSubsystem: all slots reset"));
}

FString UCSPlayerSlotSubsystem::MakePlayerKey(APlayerController* PC)
{
    if (!PC) return FString();

    int32 LpId = 0;
    if (const ULocalPlayer* LP = PC->GetLocalPlayer())
    {
        LpId = LP->GetControllerId();
    }

    // 서버가 보는 "이 연결이 누구인가" 만으로 키를 만든다. 트래블을 넘어가도 변하지 않는다.
    //  - 리슨 서버에서 호스트의 PlayerController 는 서버 쪽에서 항상 IsLocalController()==true,
    //    원격 클라를 위해 스폰된 PC 는 항상 false 다. 이 분류는 연결의 역할 자체라
    //    non-seamless ServerTravel 을 넘어가도 유지된다.
    //  - ULocalPlayer 와 그 ControllerId 는 GameInstance 소유라 트래블을 넘어 살아남는다.
    //  - PC 포인터는 쓰지 않는다. 트래블마다 바뀌어 죽은 항목이 슬롯을 차지한다.
    //
    // UniqueNetId 는 일부러 쓰지 않는다. 예전엔 이걸 1순위 키로 썼고, 그게 패키징 빌드에서만
    // 1번 캐릭터가 둘 소환되던 원인이다.
    //  - EOS DeviceID 로그인은 *기기* 단위라 한 PC 에서 두 인스턴스를 띄우면 호스트와 클라가
    //    같은 ProductUserId 를 받는다. 키가 겹치면 두 번째 플레이어가 Find() 에서 첫 번째의
    //    슬롯(Player0)을 그대로 집어가고, 결과적으로 둘 다 Player0 이 된다.
    //  - 타이틀 맵에서는 미로그인, 세션 접속 후엔 로그인처럼 도중에 유효해지기도 해서
    //    맵마다 키가 바뀌는 문제도 있다.
    //  - PIE 는 EOS 로그인을 타지 않아 이 경로가 드러나지 않았다.
    //
    // 전제: 리슨 서버 + 최대 2인(슬롯도 2개) + 중도 접속 없음. 원격 연결이 둘 이상이면
    // 이 키는 더 이상 유일하지 않다. 그 경우의 마지막 방어선은 ACSGameMode 의 중복 검사다.
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
