// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CSGA_TimeRewind.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_TimeRewind : public UGameplayAbility
{
	GENERATED_BODY()
	

public:
	UCSGA_TimeRewind();

	/** Gameplay Ability 발동 시 호출 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 취소/사망/어빌리티 제거 등 어떤 경로로 끝나도 입력/중력을 복구한다 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Ability Task가 완료되었을 때 호출할 델리게이트 함수 */
	UFUNCTION()
	void OnTimeRewindFinishedDelegate();

private:
	/** ActivateAbility 에서 입력/중력을 건드렸는가 - EndAbility 복구의 멱등성 보장 */
	bool bStateApplied = false;

	/** 복구 대상 (되감기 중 아바타/컨트롤러가 바뀌어도 우리가 건드린 것만 되돌린다) */
	TWeakObjectPtr<APlayerController> DisabledPC;
	TWeakObjectPtr<class UCharacterMovementComponent> ModifiedMovement;
	float SavedGravityScale = 1.0f;
}; 
