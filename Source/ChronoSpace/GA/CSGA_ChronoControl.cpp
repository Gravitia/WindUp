// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_ChronoControl.h"
#include "GA/AT/CSAT_ChronoControl.h"
#include "GA/TA/CSTA_ChronoControl.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "ChronoSpace.h"

UCSGA_ChronoControl::UCSGA_ChronoControl()
{
	// 형제 박스 어빌리티(ReverseGravity/WeakenGravity)와 동일하게 서버 전용.
	// 미지정(=LocalPredicted)이면 원격 클라가 예측 스폰 + 서버 스폰 복제로 박스가 2개 생겼다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UCSGA_ChronoControl::UCSGA_ChronoControl(float BoxSize)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UCSGA_ChronoControl::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ActivateTask();
}

void UCSGA_ChronoControl::ActivateTask()
{
	UCSAT_ChronoControl* BoxTask = UCSAT_ChronoControl::CreateTask(this, ACSTA_ChronoControl::StaticClass());
	BoxTask->SetDurtionTime(DurationTime);
	BoxTask->OnComplete.AddDynamic(this, &UCSGA_ChronoControl::StopActivateTask);
	BoxTask->ReadyForActivation();
}

void UCSGA_ChronoControl::StopActivateTask()
{
	bool bReplicatedEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled);
}
