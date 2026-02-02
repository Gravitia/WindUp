// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSAutoTrap.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACSAutoTrap::ACSAutoTrap()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = false;
}

void ACSAutoTrap::BeginPlay()
{
    Super::BeginPlay();

    // 서버만 패턴을 진행하고, 클라는 RepNotify로 연출만 따라간다.
    if (HasAuthority())
    {
        StartServerPattern();
    }
}

void ACSAutoTrap::StartServerPattern()
{
    if (TrapSteps.Num() <= 0)
    {
        return;
    }

    CurrentStepIndex = 0;

    // BeginPlay 직후 타이밍 안정화
    GetWorldTimerManager().SetTimerForNextTick(
        this,
        &ACSAutoTrap::ExecuteCurrentStep
    );
}

void ACSAutoTrap::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSAutoTrap, CurrentStepSlot);
    DOREPLIFETIME(ACSAutoTrap, StepSerial);
}

void ACSAutoTrap::ExecuteCurrentStep()
{
    if (!HasAuthority() || TrapSteps.Num() == 0)
    {
        return;
    }

    const FTrapStep& Step = TrapSteps[CurrentStepIndex];

    // 이번 스텝 슬롯 세팅
    CurrentStepSlot = Step.StepSlot;

    // 매 스텝마다 반드시 변하도록 시리얼 증가 (uint8 overflow는 자연스럽게 순환)
    ++StepSerial;

    // 서버에서도 필요하면 실행(서버판정/사운드 등). 
    // "연출은 클라만" 원하면 아래 줄을 지워도 된다.
    OnTrapStep(CurrentStepSlot);

    // 즉시 전파(가끔 늦게 도착하는 체감 줄이기)
    ForceNetUpdate();

    // 다음 스텝 인덱스
    CurrentStepIndex = (CurrentStepIndex + 1) % TrapSteps.Num();

    // 다음 스텝 예약
    GetWorldTimerManager().SetTimer(
        TrapTimer,
        this,
        &ACSAutoTrap::ExecuteCurrentStep,
        Step.Delay,
        false
    );
}

void ACSAutoTrap::OnRep_StepSerial()
{
    // 클라에서 매 스텝마다 무조건 호출됨
    OnTrapStep(CurrentStepSlot);
}
