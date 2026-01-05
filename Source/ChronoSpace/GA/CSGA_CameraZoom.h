// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CSGA_CameraZoom.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_CameraZoom : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCSGA_CameraZoom();

protected:
	/** ¡‹ ∞≈∏Æ */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Zoom")
	float ZoomLength = 600.0f;

	/** ¡‹ ∫∏∞£ º”µµ */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Zoom")
	float ZoomSpeed = 50.0f;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;
	
};
