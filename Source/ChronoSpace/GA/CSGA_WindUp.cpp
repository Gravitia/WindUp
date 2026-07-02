// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/CSGA_WindUp.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "ChronoSpace.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemBlueprintLibrary.h"

UCSGA_WindUp::UCSGA_WindUp()
{
    // �����Ƽ �⺻ ����
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // �Է� ���� Ÿ������ ���� (G Ű�� ������ �ִ� ���� ����)
    bRetriggerInstancedAbility = false;

    // �ʱ�ȭ
    CurrentTargetPlayer = nullptr;
    CurrentParticleComponent = nullptr;
    CurrentAudioComponent = nullptr;

    if (HealingEffect)
    {
        UE_LOG(LogCS, Log, TEXT("WindUp HealingEffect loaded successfully"));
    }
}

void UCSGA_WindUp::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UE_LOG(LogCS, Log, TEXT("WindUp Ability Activated - Charging for %f seconds"), WindUpDuration);

    // ��ó �÷��̾� ã��
    ACharacter* TargetPlayer = FindNearbyPlayer();

    if (TargetPlayer)
    {
        CurrentTargetPlayer = TargetPlayer;
        StartWindUpEffect(TargetPlayer);

        UE_LOG(LogCS, Log, TEXT("WindUp started on target: %s"), *TargetPlayer->GetName());

        // 1�� �� WindUp �Ϸ� Ÿ�̸� ����
        GetWorld()->GetTimerManager().SetTimer(
            WindUpCompleteTimerHandle,
            this,
            &UCSGA_WindUp::OnWindUpComplete,
            WindUpDuration,
            false  // �ѹ��� ����
        );

        // �Ÿ� üũ Ÿ�̸� ���� (�ֱ������� �Ÿ� Ȯ��)
        GetWorld()->GetTimerManager().SetTimer(
            DistanceCheckTimerHandle,
            this,
            &UCSGA_WindUp::CheckDistanceToTarget,
            DistanceCheckInterval,
            true  // �ݺ� ����
        );
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("No nearby player found for WindUp"));
        // ��ó�� ������ ������ �����Ƽ ����
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UCSGA_WindUp::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Ability Ended (Cancelled: %s)"), bWasCancelled ? TEXT("YES") : TEXT("NO"));

    // ��� Ÿ�̸� ����
    if (GetWorld())
    {
        if (WindUpCompleteTimerHandle.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(WindUpCompleteTimerHandle);
        }
        if (DistanceCheckTimerHandle.IsValid())
        {
            GetWorld()->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
        }
    }

    // ����Ʈ ����
    StopWindUpEffect();

    CurrentTargetPlayer = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_WindUp::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Pressed"));
    // GAS�� �ڵ����� TryActivateAbility�� ȣ���մϴ�
}

void UCSGA_WindUp::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Released - Ability cancelled"));

    // Ű�� ������ �����Ƽ ��� (1�� ���� ������ ���� �ȵ�)
    if (IsActive())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true); // bWasCancelled = true
    }
}

bool UCSGA_WindUp::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // �߰� ���� üũ
    ACharacter* OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!OwnerCharacter)
    {
        return false;
    }

    // ��ó�� �÷��̾ �ִ��� üũ
    UCSGA_WindUp* NonConstThis = const_cast<UCSGA_WindUp*>(this);
    ACharacter* NearbyPlayer = NonConstThis->FindNearbyPlayer();

    if (!NearbyPlayer)
    {
        UE_LOG(LogCS, Log, TEXT("Cannot activate WindUp: No nearby player"));
        return false;
    }

    return true;
}

void UCSGA_WindUp::OnWindUpComplete()
{
    UE_LOG(LogCS, Log, TEXT("WindUp COMPLETED! Applying healing now"));

    if (!CurrentTargetPlayer)
    {
        UE_LOG(LogCS, Warning, TEXT("WindUp completed but no target player"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // ������ �Ÿ� üũ
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (OwnerCharacter)
    {
        float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), CurrentTargetPlayer->GetActorLocation());
        if (Distance > WindUpRange)
        {
            UE_LOG(LogCS, Log, TEXT("WindUp completed but target too far away: %f"), Distance);
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
            return;
        }
    }

    // ���� ����!
    ApplyHealingEffect(CurrentTargetPlayer);

    UE_LOG(LogCS, Log, TEXT("WindUp healing successfully applied to %s"), *CurrentTargetPlayer->GetName());

    // �����Ƽ ����
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCSGA_WindUp::CheckDistanceToTarget()
{
    if (!CurrentTargetPlayer)
    {
        UE_LOG(LogCS, Warning, TEXT("Distance check: No target player"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter)
    {
        return;
    }

    float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), CurrentTargetPlayer->GetActorLocation());
    if (Distance > WindUpRange)
    {
        UE_LOG(LogCS, Log, TEXT("WindUp cancelled - target too far away: %f"), Distance);
        // ������ ����� �����Ƽ ���
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // �Ÿ��� �������� ��� ����
    UE_LOG(LogCS, VeryVerbose, TEXT("Distance check OK: %f"), Distance);
}

ACharacter* UCSGA_WindUp::FindNearbyPlayer()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter)
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // ��ü ���� �� ���͵鸸 �˻�
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter); // �ڽ� ����

    bool bHit = World->OverlapMultiByObjectType(
        OverlapResults,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_Pawn),
        FCollisionShape::MakeSphere(WindUpRange),
        QueryParams
    );

    if (!bHit)
    {
        return nullptr;
    }

    // ���� ����� �÷��̾� ã��
    ACharacter* ClosestPlayer = nullptr;
    float ClosestDistance = WindUpRange + 1.0f;

    for (const FOverlapResult& Result : OverlapResults)
    {
        ACharacter* Character = Cast<ACharacter>(Result.GetActor());

        if (!Character)
        {
            continue;
        }

        // �÷��̾� ��Ʈ�ѷ��� �ִ� ĳ���͸�
        if (!Character->GetController() || !Character->GetController()->IsA<APlayerController>())
        {
            continue;
        }

        float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), Character->GetActorLocation());

        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestPlayer = Character;
        }
    }

    return ClosestPlayer;
}

void UCSGA_WindUp::StartWindUpEffect(ACharacter* TargetPlayer)
{
    if (!TargetPlayer)
    {
        return;
    }

    // ��ƼŬ ����Ʈ ����
    if (WindUpParticleEffect)
    {
        CurrentParticleComponent = UGameplayStatics::SpawnEmitterAttached(
            WindUpParticleEffect,
            TargetPlayer->GetMesh(),
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }

    // ���� ����Ʈ ����
    if (WindUpSound)
    {
        CurrentAudioComponent = UGameplayStatics::SpawnSoundAttached(
            WindUpSound,
            TargetPlayer->GetMesh(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }

    // �ִϸ��̼� ��� (������)
    if (WindUpAnimation)
    {
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
        if (OwnerCharacter && OwnerCharacter->GetMesh())
        {
            UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
            if (AnimInstance)
            {
                AnimInstance->Montage_Play(WindUpAnimation);
            }
        }
    }

    UE_LOG(LogCS, Log, TEXT("WindUp visual effects started"));
}

void UCSGA_WindUp::StopWindUpEffect()
{
    // ��ƼŬ ����Ʈ ����
    if (CurrentParticleComponent)
    {
        CurrentParticleComponent->DestroyComponent();
        CurrentParticleComponent = nullptr;
    }

    // ���� ����Ʈ ����
    if (CurrentAudioComponent)
    {
        CurrentAudioComponent->Stop();
        CurrentAudioComponent = nullptr;
    }

    // �ִϸ��̼� ����
    if (WindUpAnimation)
    {
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
        if (OwnerCharacter && OwnerCharacter->GetMesh())
        {
            UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
            if (AnimInstance && AnimInstance->Montage_IsPlaying(WindUpAnimation))
            {
                AnimInstance->Montage_Stop(0.5f, WindUpAnimation);
            }
        }
    }

    UE_LOG(LogCS, Log, TEXT("WindUp visual effects stopped"));
}

void UCSGA_WindUp::ApplyHealingEffect(ACharacter* TargetPlayer)
{
    if (!TargetPlayer || !HealingEffect)
    {
        UE_LOG(LogCS, Warning, TEXT("Invalid TargetPlayer or HealingEffect"));
        return;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPlayer);

    if (!TargetASC)
    {
        UE_LOG(LogCS, Error, TEXT("Target %s has no AbilitySystemComponent!"), *TargetPlayer->GetName());
        return;
    }

    UE_LOG(LogCS, Log, TEXT("Found ASC for %s: %s"), *TargetPlayer->GetName(), *TargetASC->GetName());

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!SourceASC)
    {
        UE_LOG(LogCS, Error, TEXT("Source ASC is null"));
        return;
    }

    FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
    ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
        HealingEffect,
        GetAbilityLevel(),
        ContextHandle
    );

    if (SpecHandle.IsValid())
    {
        // HealingAmount�� GameplayEffect�� Magnitude�� ������ ���� �ֽ��ϴ�
        // �Ǵ� GameplayEffect���� ���� ���� ����

        FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Invalid EffectSpecHandle for healing"));
    }
}