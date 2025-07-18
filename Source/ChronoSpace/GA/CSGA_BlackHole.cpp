// Fill out your copyright notice in the Description page of Project Settings.
#include "GA/CSGA_BlackHole.h"
#include "GA/AT/CSAT_BlackHoleSphere.h"
#include "GA/TA/CSTA_BlackHoleSphere.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "ChronoSpace.h"

FVector UCSGA_BlackHole::PendingTargetLocation = FVector::ZeroVector;
bool UCSGA_BlackHole::bHasPendingTargetLocation = false;

UCSGA_BlackHole::UCSGA_BlackHole()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	if (!TargetActorClass)
	{
		UE_LOG(LogCS, Log, TEXT("TargetActorClass CDO Null "));
	}
}


void UCSGA_BlackHole::SetPendingTargetLocation(const FVector& Location)
{
    PendingTargetLocation = Location;
    bHasPendingTargetLocation = true;
    UE_LOG(LogCS, Log, TEXT("BlackHole pending target location set to: %s"), *Location.ToString());
}

void UCSGA_BlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    UE_LOG(LogCS, Log, TEXT("UCSGA_BlackHole ActivateAbility"));

    // 대기 중인 위치 정보 확인
    FVector TargetLocation = FVector::ZeroVector;
    bool bHasValidLocation = bHasPendingTargetLocation;

    if (bHasPendingTargetLocation)
    {
        TargetLocation = PendingTargetLocation;
        UE_LOG(LogCS, Log, TEXT("BlackHole using pending target location: %s"), *TargetLocation.ToString());

        // 사용 후 리셋
        bHasPendingTargetLocation = false;
        PendingTargetLocation = FVector::ZeroVector;
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("No pending target location, using player location"));
    }

    ActivateTask(TargetLocation, bHasValidLocation);
}

void UCSGA_BlackHole::ActivateTask(const FVector& TargetLocation, bool bHasValidLocation)
{
    if (!TargetActorClass)
    {
        UE_LOG(LogCS, Log, TEXT("TargetActorClass ActivateTask Null "));
        return;
    }

    UE_LOG(LogCS, Log, TEXT("Using Target Actor Class"));
    UCSAT_BlackHoleSphere* SphereTask = UCSAT_BlackHoleSphere::CreateTask(this, TargetActorClass);

    // 유효한 위치가 있으면 설정
    if (bHasValidLocation)
    {
        SphereTask->SetTargetLocation(TargetLocation);
    }

    SphereTask->SetDurtionTime(DurationTime);
    SphereTask->OnComplete.AddDynamic(this, &UCSGA_BlackHole::StopActivateTask);
    SphereTask->ReadyForActivation();
}

void UCSGA_BlackHole::StopActivateTask()
{
	bool bReplicatedEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled);
}