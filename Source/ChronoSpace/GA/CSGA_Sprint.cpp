// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_Sprint.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "ChronoSpace.h"
#include "AbilitySystemComponent.h"

UCSGA_Sprint::UCSGA_Sprint()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UCSGA_Sprint::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) return;

    UE_LOG(LogTemp, Log, TEXT("ActivateAbility Sprint"));

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(Character))
        {
            Character->GetCharacterMovement()->MaxWalkSpeed = CSCharacter->DashSpeed;
        }
    }

    // Sprint 비용 Effect 적용 및 Handle 저장
    if (SprintCostEffect && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(SprintCostEffect, GetAbilityLevel());
        if (CostSpecHandle.IsValid())
        {
            // Handle을 저장해서 나중에 제거할 수 있도록 함
            SprintCostEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpecHandle);

            if (SprintCostEffectHandle.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("Sprint Cost Effect Applied"));
            }
        }
    }
}

void UCSGA_Sprint::InputReleased(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// EndAbility 수정
void UCSGA_Sprint::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // 속도 복원
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character)
    {
        if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(Character))
        {
            Character->GetCharacterMovement()->MaxWalkSpeed = CSCharacter->WalkSpeed;
        }
    }

    // Sprint Cost Effect 강제 제거 (Duration이 남아있어도)
    if (SprintCostEffectHandle.IsValid() && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(SprintCostEffectHandle);
        SprintCostEffectHandle = FActiveGameplayEffectHandle(); // Handle 초기화
        UE_LOG(LogTemp, Log, TEXT("Sprint Cost Effect Removed Early"));
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}