// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/AT/CSAT_BlackHoleSphere.h"
#include "GA/TA/CSTA_BlackHoleSphere.h"
#include "AbilitySystemComponent.h"
#include "ChronoSpace.h"

UCSAT_BlackHoleSphere::UCSAT_BlackHoleSphere()
{
}

UCSAT_BlackHoleSphere* UCSAT_BlackHoleSphere::CreateTask(UGameplayAbility* OwningAbility, TSubclassOf<class ACSTA_BlackHoleSphere> InTargetActorClass)
{
	UCSAT_BlackHoleSphere* NewTask = NewAbilityTask<UCSAT_BlackHoleSphere>(OwningAbility);
	NewTask->TargetActorClass = InTargetActorClass;

	return NewTask;
}

void UCSAT_BlackHoleSphere::Activate()
{
	Super::Activate();
	UE_LOG(LogCS, Log, TEXT("UCSAT_BlackHoleSphere Activate"));
	SpawnAndInitializeTargetActor();
	FinalizeTargetActor();

	SetWaitingOnAvatar();
}

void UCSAT_BlackHoleSphere::OnDestroy(bool AbilityEnded)
{
	if (SpawnedTargetActor)
	{
		SpawnedTargetActor->Destroy();
	}

	Super::OnDestroy(AbilityEnded);
}

void UCSAT_BlackHoleSphere::SpawnAndInitializeTargetActor()
{
	SpawnedTargetActor = Cast<ACSTA_BlackHoleSphere>(GetWorld()->SpawnActorDeferred<ACSTA_BlackHoleSphere>(TargetActorClass, FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

	if (SpawnedTargetActor)
	{
		SpawnedTargetActor->SetOwner(GetOwnerActor());
		SpawnedTargetActor->OnComplete.AddDynamic(this, &UCSAT_BlackHoleSphere::OnTargetActorReadyCallback);
	}
}


void UCSAT_BlackHoleSphere::SetTargetLocation(const FVector& InTargetLocation)
{
    TargetLocation = InTargetLocation;
    bHasTargetLocation = true;
    UE_LOG(LogCS, Log, TEXT("BlackHole target location set to: %s"), *TargetLocation.ToString());
}

void UCSAT_BlackHoleSphere::FinalizeTargetActor()
{
    UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
    if (ASC)
    {
        // 목표 위치가 설정되어 있으면 해당 위치에, 아니면 플레이어 위치에 스폰
        FTransform SpawnTransform;
        if (bHasTargetLocation)
        {
            SpawnTransform = FTransform(TargetLocation);
            UE_LOG(LogCS, Log, TEXT("Spawning BlackHole at target location: %s"), *TargetLocation.ToString());
        }
        else
        {
            SpawnTransform = ASC->GetAvatarActor()->GetTransform();
            UE_LOG(LogCS, Log, TEXT("Spawning BlackHole at player location (no target set)"));
        }

        if (SpawnedTargetActor == nullptr)
        {
            UE_LOG(LogCS, Log, TEXT("SpawnedTargetActor Not Found"));
            return;
        }

        SpawnedTargetActor->FinishSpawning(SpawnTransform);
        ASC->SpawnedTargetActors.Add(SpawnedTargetActor);
        SpawnedTargetActor->StartTargeting(Ability);
        GetWorld()->GetTimerManager().SetTimer(EndTimer, this, &UCSAT_BlackHoleSphere::EndTargetActor, DurationTime, false);
    }
}

void UCSAT_BlackHoleSphere::EndTargetActor()
{
	SpawnedTargetActor->ConfirmTargeting();
}

