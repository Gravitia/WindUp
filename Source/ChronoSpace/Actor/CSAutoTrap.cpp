// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSAutoTrap.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACSAutoTrap::ACSAutoTrap()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
}

void ACSAutoTrap::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && TrapSteps.Num() > 0)
    {
        CurrentStepIndex = 0;

        // BeginPlay 타이밍 안정화
        GetWorldTimerManager().SetTimerForNextTick(
            this,
            &ACSAutoTrap::ExecuteCurrentStep
        );
    }
}

void ACSAutoTrap::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSAutoTrap, CurrentStepSlot);
}

void ACSAutoTrap::ExecuteCurrentStep()
{
    if (!HasAuthority() || TrapSteps.Num() == 0)
        return;

    const FTrapStep& Step = TrapSteps[CurrentStepIndex];

    CurrentStepSlot = Step.StepSlot;

    // BP에 스텝 전달 (행동 해석은 BP에서)
    OnTrapStep(CurrentStepSlot);

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

void ACSAutoTrap::OnRep_CurrentStep()
{
    // 상태 기반 연출만 처리
    OnTrapStep(CurrentStepSlot);
}