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
    // WindUp �Ϸ� �Լ� (1�� �� ȣ��)
    UFUNCTION()
    void OnWindUpComplete();

    // �Ÿ� üũ �Լ� (0.1�ʸ��� ȣ��)
    UFUNCTION()
    void CheckDistanceToTarget();

    // ��ó �÷��̾� ã��
    UFUNCTION(BlueprintCallable)
    ACharacter* FindNearbyPlayer();

    // �¿� ���� ����/����
    UFUNCTION(BlueprintCallable)
    void StartWindUpEffect(ACharacter* TargetPlayer);

    UFUNCTION(BlueprintCallable)
    void StopWindUpEffect();

    // GameplayEffect ����
    void ApplyHealingEffect(ACharacter* TargetPlayer);

protected:
    // === WindUp ���� ===
    // WindUp ���� (����: cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpRange = 300.0f;

    // WindUp �Ϸ���� �ʿ��� �ð� (��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float WindUpDuration = 1.0f;

    // �Ÿ� üũ ���� (��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Settings")
    float DistanceCheckInterval = 0.1f;

    // === GameplayEffect ===
    // ü�� ȸ�� GameplayEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    TSubclassOf<class UGameplayEffect> HealingEffect;

    // === �ð�/û�� ȿ�� ===
    // WindUp ����Ʈ ��ƼŬ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UParticleSystem* WindUpParticleEffect;

    // WindUp ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class USoundBase* WindUpSound;

    // WindUp �ִϸ��̼�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindUp Effects")
    class UAnimMontage* WindUpAnimation;

private:
    // Ÿ�̸� �ڵ��
    FTimerHandle WindUpCompleteTimerHandle;  // 1�� �� �Ϸ�
    FTimerHandle DistanceCheckTimerHandle;   // �Ÿ� üũ��

    // ���� Ÿ�� �÷��̾�
    UPROPERTY()
    ACharacter* CurrentTargetPlayer;

    // ����Ʈ ������Ʈ��
    UPROPERTY()
    class UParticleSystemComponent* CurrentParticleComponent;

    UPROPERTY()
    class UAudioComponent* CurrentAudioComponent;
};
