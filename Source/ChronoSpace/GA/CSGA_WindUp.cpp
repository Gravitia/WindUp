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
    // 어빌리티 기본 설정
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 입력 유지 타입으로 설정 (G 키를 누르고 있는 동안 지속)
    bRetriggerInstancedAbility = false;

    // 초기화
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

    // 근처 플레이어 찾기
    ACharacter* TargetPlayer = FindNearbyPlayer();

    if (TargetPlayer)
    {
        CurrentTargetPlayer = TargetPlayer;
        StartWindUpEffect(TargetPlayer);

        UE_LOG(LogCS, Log, TEXT("WindUp started on target: %s"), *TargetPlayer->GetName());

        // 1초 후 WindUp 완료 타이머 시작
        GetWorld()->GetTimerManager().SetTimer(
            WindUpCompleteTimerHandle,
            this,
            &UCSGA_WindUp::OnWindUpComplete,
            WindUpDuration,
            false  // 한번만 실행
        );

        // 거리 체크 타이머 시작 (주기적으로 거리 확인)
        GetWorld()->GetTimerManager().SetTimer(
            DistanceCheckTimerHandle,
            this,
            &UCSGA_WindUp::CheckDistanceToTarget,
            DistanceCheckInterval,
            true  // 반복 실행
        );
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("No nearby player found for WindUp"));
        // 근처에 팀원이 없으면 어빌리티 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UCSGA_WindUp::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Ability Ended (Cancelled: %s)"), bWasCancelled ? TEXT("YES") : TEXT("NO"));

    // 모든 타이머 정리
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

    // 이펙트 정리
    StopWindUpEffect();

    CurrentTargetPlayer = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_WindUp::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Pressed"));
    // GAS가 자동으로 TryActivateAbility를 호출합니다
}

void UCSGA_WindUp::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Released - Ability cancelled"));

    // 키를 놓으면 어빌리티 취소 (1초 전에 놓으면 힐링 안됨)
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

    // 추가 조건 체크
    ACharacter* OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!OwnerCharacter)
    {
        return false;
    }

    // 근처에 플레이어가 있는지 체크
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

    // 마지막 거리 체크
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

    // 힐링 적용!
    ApplyHealingEffect(CurrentTargetPlayer);

    UE_LOG(LogCS, Log, TEXT("WindUp healing successfully applied to %s"), *CurrentTargetPlayer->GetName());

    // 어빌리티 종료
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
        // 범위를 벗어나면 어빌리티 취소
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 거리가 괜찮으면 계속 진행
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

    // 구체 범위 내 액터들만 검색
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter); // 자신 제외

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

    // 가장 가까운 플레이어 찾기
    ACharacter* ClosestPlayer = nullptr;
    float ClosestDistance = WindUpRange + 1.0f;

    for (const FOverlapResult& Result : OverlapResults)
    {
        ACharacter* Character = Cast<ACharacter>(Result.GetActor());

        if (!Character)
        {
            continue;
        }

        // 플레이어 컨트롤러가 있는 캐릭터만
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

    // 파티클 이펙트 시작
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

    // 사운드 이펙트 시작
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

    // 애니메이션 재생 (시전자)
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
    // 파티클 이펙트 정지
    if (CurrentParticleComponent)
    {
        CurrentParticleComponent->DestroyComponent();
        CurrentParticleComponent = nullptr;
    }

    // 사운드 이펙트 정지
    if (CurrentAudioComponent)
    {
        CurrentAudioComponent->Stop();
        CurrentAudioComponent = nullptr;
    }

    // 애니메이션 정지
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
        // HealingAmount를 GameplayEffect의 Magnitude에 설정할 수도 있습니다
        // 또는 GameplayEffect에서 직접 값을 설정

        FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Invalid EffectSpecHandle for healing"));
    }
}