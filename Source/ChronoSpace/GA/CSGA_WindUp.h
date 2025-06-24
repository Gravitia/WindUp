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
    // �¿� ���� ƽ �Լ�
    UFUNCTION()
    void OnWindUpTick();

    // ��ó �÷��̾� ã��
    UFUNCTION(BlueprintCallable)
    ACharacter* FindNearbyPlayer();

    // �¿� ���� ����/����
    UFUNCTION(BlueprintCallable)
    void StartWindUpEffect(ACharacter* TargetPlayer);

    UFUNCTION(BlueprintCallable)
    void StopWindUpEffect();

    // GameplayEffect ����/����
    void ApplyHealingEffect(ACharacter* TargetPlayer);
    void RemoveHealingEffect();

protected:
    // === �¿� ���� ���� ===

    // �¿� ���� ���� (����: cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpRange = 300.0f;

    // �ʴ� ȸ���� ü��
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float HealthPerSecond = 10.0f;

    // �¿� ���� ƽ ���� (��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float TickInterval = 0.2f;

    // === GameplayEffect ===

    // ü�� ȸ�� GameplayEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    TSubclassOf<class UGameplayEffect> HealingEffect;

    // === �ð�/û�� ȿ�� ===

    // �¿� ���� ����Ʈ ��ƼŬ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UParticleSystem* WindUpParticleEffect;

    // �¿� ���� ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class USoundBase* WindUpSound;

    // �¿� ���� �ִϸ��̼�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UAnimMontage* WindUpAnimation;

private:
    // Ÿ�̸� �ڵ�
    FTimerHandle WindUpTimerHandle;

    // ���� Ÿ�� �÷��̾�
    UPROPERTY()
    ACharacter* CurrentTargetPlayer;

    // ���� ����� GameplayEffect �ڵ�
    FActiveGameplayEffectHandle CurrentHealingEffectHandle;

    // ����Ʈ ������Ʈ��
    UPROPERTY()
    class UParticleSystemComponent* CurrentParticleComponent;

    UPROPERTY()
    class UAudioComponent* CurrentAudioComponent;
};
