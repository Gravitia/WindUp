// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CSGA_GravityCore.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_GravityCore : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UCSGA_GravityCore();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void OnCore();
	void OffCore();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Core")
	TSubclassOf<class ACSGravityCoreSphere> GravityCoreClass;

	UPROPERTY()
	TObjectPtr<class ACSGravityCoreSphere> GravityCore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	float CoreScale = 1.0f;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Check Component")
	bool bCheckMeshComponentAffectedByGravityCore = false;


	/* Sound */
	UPROPERTY()
	UAudioComponent* GravityCoreOnAudioComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* GravityCoreOnSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* GravityCoreOffSound;
};
