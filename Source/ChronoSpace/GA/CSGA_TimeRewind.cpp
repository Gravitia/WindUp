#include "GA/CSGA_TimeRewind.h"
#include "GA/AT/CSAT_TimeRewind.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"  // GameplayTag 사용을 위한 헤더 추가
#include "Abilities/GameplayAbility.h"  // BlockAbilitiesWithTag 사용을 위해 필요
#include "Character/CSCharacterPlayer.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"  // 중력 조절을 위해 필요 
#include "ActorComponent/CSTransformRecordComponent.h"

UCSGA_TimeRewind::UCSGA_TimeRewind()
{
    // 양측 실행(의도): 서버와 소유 클라가 각자 자기 기록을 따라 되감는다. 클라가 자기 캐릭터를 직접 움직여야
    // 되감기가 부드럽다 (ServerOnly 로 바꾸면 원격 클라는 서버 보정으로 끌려가며 떨린다).
    // 그 대신 ActivateAbility 에서 건드린 입력/중력은 EndAbility 에서 반드시 되돌린다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

    // 태그 설정
    FGameplayTagContainer AbilityTagsContainer;
    AbilityTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.TimeRewind")));
    SetAssetTags(AbilityTagsContainer);

    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement")));
}


void UCSGA_TimeRewind::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    UE_LOG(LogTemp, Log, TEXT("GA Active"));

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    if (!AvatarActor)
    {
        UE_LOG(LogTemp, Error, TEXT("AvatarActor is null! Ending ability."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ACharacter* PlayerCharacter = Cast<ACharacter>(AvatarActor); 
    UCSTransformRecordComponent* TransformRecordComponent = AvatarActor->GetComponentByClass<UCSTransformRecordComponent>(); 

    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("TimeRewind: avatar is not a Character"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 되감을 프레임 수는 기록 컴포넌트의 상한을 따른다 (예전엔 99 를 따로 하드코딩해 MaxHistorySize 를 낮추면 영원히 발동하지 않았다)
    const int32 RewindFrameCount = TransformRecordComponent ? TransformRecordComponent->GetMaxHistorySize() : 0;
    if (!TransformRecordComponent || RewindFrameCount <= 0 || TransformRecordComponent->GetTransformHistory().Num() < RewindFrameCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("Not enough Transform History Available"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 입력 비활성화 - 누구를 껐는지 기억해 EndAbility 에서 같은 대상만 켠다
    if (APlayerController* PC = Cast<APlayerController>(PlayerCharacter->GetController()))
    {
        PC->DisableInput(PC);
        DisabledPC = PC;
        UE_LOG(LogTemp, Log, TEXT("Player input disabled during Time Rewind."));
    }

    // 중력 비활성화 - 원래 값을 저장해 EndAbility 에서 복구
    if (UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement())
    {
        SavedGravityScale = MovementComp->GravityScale;
        MovementComp->GravityScale = 0.0f;
        MovementComp->StopMovementImmediately(); // 현재 속도 정지
        ModifiedMovement = MovementComp;
        UE_LOG(LogTemp, Log, TEXT("Gravity disabled during Time Rewind."));
    }
    bStateApplied = true;

    // Ability Task 생성 (0.5초 동안 되감기)
    TArray<FCSF_CharacterFrameData> RewindFrames;
    const TArray<FCSF_CharacterFrameData>& History = TransformRecordComponent->GetTransformHistory();
    for (int32 i = History.Num() - RewindFrameCount; i < History.Num(); i++)
    {
        RewindFrames.Add(History[i]);
    }

    UCSAT_TimeRewind* TimeRewindTask = UCSAT_TimeRewind::CreateTimeRewindTask(
        this,
        PlayerCharacter,
        RewindFrames,
        0.5f
    );

    TimeRewindTask->OnTimeRewindFinished.AddDynamic(this, &UCSGA_TimeRewind::OnTimeRewindFinishedDelegate);
    TimeRewindTask->ReadyForActivation();
}

void UCSGA_TimeRewind::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{

}

void UCSGA_TimeRewind::OnTimeRewindFinishedDelegate()
{
    UE_LOG(LogTemp, Log, TEXT("Time rewind finished. Ending ability."));
    // 복구는 EndAbility 가 담당 (정상 종료/취소/사망 모두 같은 경로)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UCSGA_TimeRewind::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // 예전엔 복구가 태스크 완료 델리게이트에만 있어서 취소/사망으로 끝나면 입력 비활성 + 중력 0 이 영구 고착됐다.
    if (bStateApplied)
    {
        bStateApplied = false;

        if (APlayerController* PC = DisabledPC.Get())
        {
            PC->EnableInput(PC);
            UE_LOG(LogTemp, Log, TEXT("Player input re-enabled after Time Rewind."));
        }
        DisabledPC = nullptr;

        if (UCharacterMovementComponent* MovementComp = ModifiedMovement.Get())
        {
            MovementComp->GravityScale = SavedGravityScale;
            UE_LOG(LogTemp, Log, TEXT("Gravity re-enabled after Time Rewind."));
        }
        ModifiedMovement = nullptr;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}