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

    // �����Ƽ �±� ���� (���û���)
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.WindUp")));

    // �ʱ�ȭ
    CurrentTargetPlayer = nullptr;
    CurrentParticleComponent = nullptr;
    CurrentAudioComponent = nullptr;

    // �⺻ GameplayEffect �ε� (�������Ʈ���� �����ϰų� ���⼭ �ε�)
    static ConstructorHelpers::FClassFinder<UGameplayEffect> HealingEffectRef(
        TEXT("/Game/01_Blueprint/GA/GE/BPGE_WindUpHealing.BPGE_WindUpHealing_C")
    );
    if (HealingEffectRef.Succeeded())
    {
        HealingEffect = HealingEffectRef.Class;
        UE_LOG(LogCS, Log, TEXT("WindUp HealingEffect loaded successfully"));
    }
}

void UCSGA_WindUp::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UE_LOG(LogCS, Log, TEXT("WindUp Ability Activated"));

    // ��ó �÷��̾� ã��
    ACharacter* TargetPlayer = FindNearbyPlayer();

    if (TargetPlayer)
    {
        CurrentTargetPlayer = TargetPlayer;
        StartWindUpEffect(TargetPlayer);

        UE_LOG(LogCS, Log, TEXT("WindUp started on target: %s"), *TargetPlayer->GetName());

        // Ÿ�̸ӷ� �ֱ������� ü�� ȸ�� �� �Ÿ� üũ
        GetWorld()->GetTimerManager().SetTimer(
            WindUpTimerHandle,
            this,
            &UCSGA_WindUp::OnWindUpTick,
            TickInterval,
            true  // �ݺ�
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
    UE_LOG(LogCS, Log, TEXT("WindUp Ability Ended"));

    // Ÿ�̸� ����
    if (GetWorld() && WindUpTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(WindUpTimerHandle);
    }

    // ����Ʈ ����
    StopWindUpEffect();
    RemoveHealingEffect();

    CurrentTargetPlayer = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_WindUp::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Pressed"));

    // WindUp�� ������ �����Ƽ�̹Ƿ� InputPressed������ Ư���� ó�� ����
    // GAS�� �ڵ����� TryActivateAbility�� ȣ�����ݴϴ�
}

void UCSGA_WindUp::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Released"));

    // �����Ƽ�� Ȱ��ȭ�Ǿ� �ִٸ� ����
    if (IsActive())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
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
    // �ӽ÷� const_cast ��� (�����δ� const �Լ����� �������� ����)
    UCSGA_WindUp* NonConstThis = const_cast<UCSGA_WindUp*>(this);
    ACharacter* NearbyPlayer = NonConstThis->FindNearbyPlayer();

    if (!NearbyPlayer)
    {
        UE_LOG(LogCS, Log, TEXT("Cannot activate WindUp: No nearby player"));
        return false;
    }

    return true;
}

void UCSGA_WindUp::OnWindUpTick()
{
    if (!CurrentTargetPlayer)
    {
        UE_LOG(LogCS, Warning, TEXT("WindUp tick: No target player"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // �����: Ÿ�� ���� �ڼ��� �α�
    UE_LOG(LogCS, Log, TEXT("=== Target Debug Info ==="));
    UE_LOG(LogCS, Log, TEXT("Target Name: %s"), *CurrentTargetPlayer->GetName());
    UE_LOG(LogCS, Log, TEXT("Target Class: %s"), *CurrentTargetPlayer->GetClass()->GetName());
    UE_LOG(LogCS, Log, TEXT("Has Controller: %s"), CurrentTargetPlayer->GetController() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogCS, Log, TEXT("Has PlayerState: %s"), CurrentTargetPlayer->GetPlayerState() ? TEXT("YES") : TEXT("NO"));

    // �Ÿ� üũ
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter)
    {
        return;
    }

    float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), CurrentTargetPlayer->GetActorLocation());
    if (Distance > WindUpRange)
    {
        UE_LOG(LogCS, Log, TEXT("WindUp target too far away: %f"), Distance);
        // ������ ����� �����Ƽ ����
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // GameplayEffect�� ���� ü�� ȸ��
    ApplyHealingEffect(CurrentTargetPlayer);

    UE_LOG(LogCS, Log, TEXT("WindUp healing applied to %s"), *CurrentTargetPlayer->GetName());
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

    // ��ü ���� �� ���͵鸸 �˻� (�ξ� ȿ����!)
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter); // �ڽ� ����

    // ���� �� ��� Pawn Ÿ�� ���� �˻�
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

        // ĳ���Ͱ� �ƴϸ� ��ŵ
        if (!Character)
        {
            continue;
        }

        // �÷��̾� ��Ʈ�ѷ��� �ִ� ĳ���͸� (���� �÷��̾�)
        if (!Character->GetController() || !Character->GetController()->IsA<APlayerController>())
        {
            continue;
        }

        // �Ÿ� ��� (��Ȯ�� �Ÿ�)
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

    // ��� 1: UAbilitySystemBlueprintLibrary ��� (�� ������)
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPlayer);


    if (!TargetASC)
    {
        UE_LOG(LogCS, Error, TEXT("Target %s has no AbilitySystemComponent anywhere!"), *TargetPlayer->GetName());
        UE_LOG(LogCS, Error, TEXT("Target Controller: %s"), TargetPlayer->GetController() ? *TargetPlayer->GetController()->GetName() : TEXT("NULL"));
        UE_LOG(LogCS, Error, TEXT("Target PlayerState: %s"), TargetPlayer->GetPlayerState() ? *TargetPlayer->GetPlayerState()->GetName() : TEXT("NULL"));

        return;
    }

    UE_LOG(LogCS, Log, TEXT("Found ASC for %s: %s"), *TargetPlayer->GetName(), *TargetASC->GetName());

    // GameplayEffect ����
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
        FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

        if (ActiveHandle.IsValid())
        {
            UE_LOG(LogCS, Log, TEXT("Healing effect successfully applied to %s"), *TargetPlayer->GetName());
        }
        else
        {
            UE_LOG(LogCS, Log, TEXT("Failed to apply healing effect to %s, Maybe WindUp CoolTime"), *TargetPlayer->GetName());
        }
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Invalid EffectSpecHandle for healing"));
    }
}

void UCSGA_WindUp::RemoveHealingEffect()
{
    // �������� ȿ���� �ִٸ� ����
    if (CurrentHealingEffectHandle.IsValid() && CurrentTargetPlayer)
    {
        UAbilitySystemComponent* TargetASC = CurrentTargetPlayer->FindComponentByClass<UAbilitySystemComponent>();
        if (TargetASC)
        {
            TargetASC->RemoveActiveGameplayEffect(CurrentHealingEffectHandle);
            CurrentHealingEffectHandle = FActiveGameplayEffectHandle();
        }
    }
}