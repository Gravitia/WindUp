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

    // 어빌리티 태그 설정 (선택사항)
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.WindUp")));

    // 초기화
    CurrentTargetPlayer = nullptr;
    CurrentParticleComponent = nullptr;
    CurrentAudioComponent = nullptr;

    // 기본 GameplayEffect 로드 (블루프린트에서 설정하거나 여기서 로드)
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

    // 근처 플레이어 찾기
    ACharacter* TargetPlayer = FindNearbyPlayer();

    if (TargetPlayer)
    {
        CurrentTargetPlayer = TargetPlayer;
        StartWindUpEffect(TargetPlayer);

        UE_LOG(LogCS, Log, TEXT("WindUp started on target: %s"), *TargetPlayer->GetName());

        // 타이머로 주기적으로 체력 회복 및 거리 체크
        GetWorld()->GetTimerManager().SetTimer(
            WindUpTimerHandle,
            this,
            &UCSGA_WindUp::OnWindUpTick,
            TickInterval,
            true  // 반복
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
    UE_LOG(LogCS, Log, TEXT("WindUp Ability Ended"));

    // 타이머 정리
    if (GetWorld() && WindUpTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(WindUpTimerHandle);
    }

    // 이펙트 정리
    StopWindUpEffect();
    RemoveHealingEffect();

    CurrentTargetPlayer = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_WindUp::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Pressed"));

    // WindUp은 지속형 어빌리티이므로 InputPressed에서는 특별한 처리 없음
    // GAS가 자동으로 TryActivateAbility를 호출해줍니다
}

void UCSGA_WindUp::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogCS, Log, TEXT("WindUp Input Released"));

    // 어빌리티가 활성화되어 있다면 종료
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

    // 추가 조건 체크
    ACharacter* OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!OwnerCharacter)
    {
        return false;
    }

    // 근처에 플레이어가 있는지 체크
    // 임시로 const_cast 사용 (실제로는 const 함수에서 수정하지 않음)
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

    // 디버깅: 타겟 정보 자세히 로깅
    UE_LOG(LogCS, Log, TEXT("=== Target Debug Info ==="));
    UE_LOG(LogCS, Log, TEXT("Target Name: %s"), *CurrentTargetPlayer->GetName());
    UE_LOG(LogCS, Log, TEXT("Target Class: %s"), *CurrentTargetPlayer->GetClass()->GetName());
    UE_LOG(LogCS, Log, TEXT("Has Controller: %s"), CurrentTargetPlayer->GetController() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogCS, Log, TEXT("Has PlayerState: %s"), CurrentTargetPlayer->GetPlayerState() ? TEXT("YES") : TEXT("NO"));

    // 거리 체크
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter)
    {
        return;
    }

    float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), CurrentTargetPlayer->GetActorLocation());
    if (Distance > WindUpRange)
    {
        UE_LOG(LogCS, Log, TEXT("WindUp target too far away: %f"), Distance);
        // 범위를 벗어나면 어빌리티 종료
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // GameplayEffect를 통한 체력 회복
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

    // 구체 범위 내 액터들만 검색 (훨씬 효율적!)
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter); // 자신 제외

    // 범위 내 모든 Pawn 타입 액터 검색
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

        // 캐릭터가 아니면 스킵
        if (!Character)
        {
            continue;
        }

        // 플레이어 컨트롤러가 있는 캐릭터만 (실제 플레이어)
        if (!Character->GetController() || !Character->GetController()->IsA<APlayerController>())
        {
            continue;
        }

        // 거리 계산 (정확한 거리)
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

    // 방법 1: UAbilitySystemBlueprintLibrary 사용 (더 안전함)
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetPlayer);


    if (!TargetASC)
    {
        UE_LOG(LogCS, Error, TEXT("Target %s has no AbilitySystemComponent anywhere!"), *TargetPlayer->GetName());
        UE_LOG(LogCS, Error, TEXT("Target Controller: %s"), TargetPlayer->GetController() ? *TargetPlayer->GetController()->GetName() : TEXT("NULL"));
        UE_LOG(LogCS, Error, TEXT("Target PlayerState: %s"), TargetPlayer->GetPlayerState() ? *TargetPlayer->GetPlayerState()->GetName() : TEXT("NULL"));

        return;
    }

    UE_LOG(LogCS, Log, TEXT("Found ASC for %s: %s"), *TargetPlayer->GetName(), *TargetASC->GetName());

    // GameplayEffect 적용
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
    // 지속적인 효과가 있다면 제거
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