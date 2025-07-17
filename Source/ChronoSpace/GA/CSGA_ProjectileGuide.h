// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "CSGA_ProjectileGuide.generated.h"

class UCameraComponent;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_ProjectileGuide : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UCSGA_ProjectileGuide();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 가이드라인 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float GuideDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float MaxGuideDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float UpdateRate = 0.02f;

private:
	FTimerHandle UpdateTimerHandle;
	FTimerHandle DurationTimerHandle;

	void UpdateGuideLine();
	void OnGuideDurationEnd();
	FVector GetPlayerForwardDirection() const;
	FVector GetStartLocation() const;
};
