// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Character.h"
#include "Components/SphereComponent.h"
#include "CSGA_WindUp.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_WindUp : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
    UCSGA_WindUp();

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
        OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

private:
    // 태엽 감기 틱 함수
    UFUNCTION()
    void OnWindUpTick();

    // 근처 플레이어 찾기
    UFUNCTION(BlueprintCallable)
    ACharacter* FindNearbyPlayer();

    // 태엽 감기 시작/종료
    UFUNCTION(BlueprintCallable)
    void StartWindUpEffect(ACharacter* TargetPlayer);

    UFUNCTION(BlueprintCallable)
    void StopWindUpEffect();

    // GameplayEffect 적용/제거
    void ApplyHealingEffect(ACharacter* TargetPlayer);
    void RemoveHealingEffect();

protected:
    // === 태엽 감기 설정 ===

    // 태엽 감기 범위 (단위: cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpRange = 300.0f;

    // 초당 회복할 체력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float HealthPerSecond = 10.0f;

    // 태엽 감기 틱 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float TickInterval = 0.2f;

    // === GameplayEffect ===

    // 체력 회복 GameplayEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    TSubclassOf<class UGameplayEffect> HealingEffect;

    // === 시각/청각 효과 ===

    // 태엽 감기 이펙트 파티클
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UParticleSystem* WindUpParticleEffect;

    // 태엽 감기 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class USoundBase* WindUpSound;

    // 태엽 감기 애니메이션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UAnimMontage* WindUpAnimation;

private:
    // 타이머 핸들
    FTimerHandle WindUpTimerHandle;

    // 현재 타겟 플레이어
    UPROPERTY()
    ACharacter* CurrentTargetPlayer;

    // 현재 적용된 GameplayEffect 핸들
    FActiveGameplayEffectHandle CurrentHealingEffectHandle;

    // 이펙트 컴포넌트들
    UPROPERTY()
    class UParticleSystemComponent* CurrentParticleComponent;

    UPROPERTY()
    class UAudioComponent* CurrentAudioComponent;
};
