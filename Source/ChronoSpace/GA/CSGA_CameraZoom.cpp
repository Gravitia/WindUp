// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_CameraZoom.h"
#include "Character/CSCharacterPlayer.h"
#include "ChronoSpace.h"

UCSGA_CameraZoom::UCSGA_CameraZoom()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	bRetriggerInstancedAbility = false;

}

void UCSGA_CameraZoom::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor.Get());
	if (!CSPlayer)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CSPlayer->ZoomCamera(ZoomLength, ZoomSpeed);

	UE_LOG(LogCS, Log, TEXT("CameraZoom Ability Activated"));
}

void UCSGA_CameraZoom::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor.Get());
	if (CSPlayer)
	{
		CSPlayer->ZoomCamera(0.f, ZoomSpeed);
	}

	UE_LOG(LogCS, Log, TEXT("CameraZoom Ability Ended"));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}