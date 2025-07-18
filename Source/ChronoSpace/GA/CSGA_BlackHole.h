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
	void ActivateTask(const FVector& TargetLocation = FVector::ZeroVector, bool bHasValidLocation = false);

	UFUNCTION()
	void StopActivateTask();

	UPROPERTY(EditAnywhere, Category = "Duration")
	float DurationTime = 10.0f;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target Actor")
	TSubclassOf<class ACSTA_BlackHoleSphere> TargetActorClass;

public:
	// 정적 변수로 위치 정보 전달
	static FVector PendingTargetLocation;
	static bool bHasPendingTargetLocation;

	// 위치 설정 함수
	static void SetPendingTargetLocation(const FVector& Location);
}; 
