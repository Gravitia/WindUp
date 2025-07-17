// Fill out your copyright notice in the Description page of Project Settings.
#include "GA/CSGA_BlackHole.h"
#include "GA/AT/CSAT_BlackHoleSphere.h"
#include "GA/TA/CSTA_BlackHoleSphere.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "ChronoSpace.h"

UCSGA_BlackHole::UCSGA_BlackHole()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	if (!TargetActorClass)
	{
		UE_LOG(LogCS, Log, TEXT("TargetActorClass CDO Null "));
	}

	/*
	if (!TargetActorClass) // 에디터에서 설정되지 않은 경우만
	{
		static ConstructorHelpers::FClassFinder<ACSTA_BlackHoleSphere> DefaultClass(
			TEXT("/Game/01_Blueprint/GA/TA/BPTA_BlackHoleSphere.BPTA_BlackHoleSphere_C")
		);
		if (DefaultClass.Succeeded())
		{
			TargetActorClass = DefaultClass.Class;
			UE_LOG(LogCS, Log, TEXT("Using default TargetActorClass"));
		}
	}
	else
	{
		UE_LOG(LogCS, Log, TEXT("Using editor-configured TargetActorClass"));
	}
	*/
}

void UCSGA_BlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	UE_LOG(LogCS, Log, TEXT("UCSGA_BlackHole ActivateAbility"));
	ActivateTask();
}

void UCSGA_BlackHole::ActivateTask()
{
	if (!TargetActorClass)
	{
		UE_LOG(LogCS, Log, TEXT("TargetActorClass ActivateTask Null "));
	}

	UE_LOG(LogCS, Log, TEXT("Using Target Actor Class"));

	UCSAT_BlackHoleSphere* SphereTask = UCSAT_BlackHoleSphere::CreateTask(this, TargetActorClass);
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