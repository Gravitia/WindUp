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
    // WindUp 완료 함수 (1초 후 호출)
    UFUNCTION()
    void OnWindUpComplete();

    // 거리 체크 함수 (0.1초마다 호출)
    UFUNCTION()
    void CheckDistanceToTarget();

    // 근처 플레이어 찾기
    UFUNCTION(BlueprintCallable)
    ACharacter* FindNearbyPlayer();

    // 태엽 감기 시작/종료
    UFUNCTION(BlueprintCallable)
    void StartWindUpEffect(ACharacter* TargetPlayer);

    UFUNCTION(BlueprintCallable)
    void StopWindUpEffect();

    // GameplayEffect 적용
    void ApplyHealingEffect(ACharacter* TargetPlayer);

protected:
    // === WindUp 설정 ===
    // WindUp 범위 (단위: cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpRange = 300.0f;

    // WindUp 완료까지 필요한 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpDuration = 1.0f;

    // 거리 체크 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float DistanceCheckInterval = 0.1f;

    // === GameplayEffect ===
    // 체력 회복 GameplayEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    TSubclassOf<class UGameplayEffect> HealingEffect;

    // === 시각/청각 효과 ===
    // WindUp 이펙트 파티클
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UParticleSystem* WindUpParticleEffect;

    // WindUp 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class USoundBase* WindUpSound;

    // WindUp 애니메이션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UAnimMontage* WindUpAnimation;

private:
    // 타이머 핸들들
    FTimerHandle WindUpCompleteTimerHandle;  // 1초 후 완료
    FTimerHandle DistanceCheckTimerHandle;   // 거리 체크용

    // 현재 타겟 플레이어
    UPROPERTY()
    ACharacter* CurrentTargetPlayer;

    // 이펙트 컴포넌트들
    UPROPERTY()
    class UParticleSystemComponent* CurrentParticleComponent;

    UPROPERTY()
    class UAudioComponent* CurrentAudioComponent;
};
