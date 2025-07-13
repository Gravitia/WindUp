// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CSGA_BlackHole.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_BlackHole : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UCSGA_BlackHole();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	FORCEINLINE const float GetDurationTime() const { return DurationTime; }
	FORCEINLINE void SetDurationTime(float InDuarionTime) { DurationTime = InDuarionTime; }

private:
	void ActivateTask();

	UFUNCTION()
	void StopActivateTask();

	// 블랙홀 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duration",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "60.0"))
	float DurationTime = 10.0f;

	// 플레이어로부터 블랙홀까지의 오프셋 (상대적 위치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole Settings",
		meta = (AllowPrivateAccess = "true"))
	FVector BlackHoleOffset = FVector(600.0f, 0.0f, 200.0f);
};
