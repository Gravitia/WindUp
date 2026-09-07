// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_CameraZoom.h"
#include "Character/CSCharacterPlayer.h"
#include "ActorComponent/CSCameraRigComponent.h"
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

	// 예전 부호 규약: 양수 ZoomLength = 팔이 짧아진다(가까이). 등속 유지를 위해 Linear
	const float BlendTime = CSCameraRig::BlendTimeFromSpeed(ZoomLength, ZoomSpeed);

	FCSCameraModifier Modifier;
	Modifier.Source = CSCameraRigSource::CameraZoomAbility;
	Modifier.ArmLengthDelta = -ZoomLength;
	Modifier.BlendInTime = BlendTime;
	Modifier.BlendOutTime = BlendTime;
	Modifier.Blend = ECSCameraBlend::Linear;

	CSPlayer->AddCameraModifier(Modifier);

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
		CSPlayer->RemoveCameraModifier(CSCameraRigSource::CameraZoomAbility);
	}

	UE_LOG(LogCS, Log, TEXT("CameraZoom Ability Ended"));

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}