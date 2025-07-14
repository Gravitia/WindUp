// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_GravityCore.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Actor/CSGravityCoreSphere.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"
#include "Character/CSCharacterPlayer.h"

UCSGA_GravityCore::UCSGA_GravityCore()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	GravityCoreClass = nullptr;
}

void UCSGA_GravityCore::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	check(GravityCoreClass);

	UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UCSGA_GravityCore - ActivateAbility"), GetWorld()->GetNetMode());

	OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		FVector SpawnLocation = OwnerCharacter->GetActorLocation();

		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnTransform.SetRotation(FQuat::Identity);
		SpawnTransform.SetScale3D(FVector(CoreScale));

		GravityCore = GetWorld()->SpawnActorDeferred<ACSGravityCoreSphere>(GravityCoreClass, SpawnTransform, OwnerCharacter, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if ( GravityCore )
		{
			UGameplayStatics::FinishSpawningActor(GravityCore, SpawnTransform);
			GravityCore->AttachToActor(ActorInfo->AvatarActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			/*Sphere = NewObject<USphereComponent>(OwnerCharacter);
			Sphere->SetSphereRadius(GravityCore->GetMeshRadius() * GravityCore->GetActorScale3D().X);
			Sphere->SetCollisionProfileName(CPROFILE_CSCAPSULE);
			Sphere->SetIsReplicated(true);
			Sphere->AttachToComponent(OwnerCharacter->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Sphere->RegisterComponent();
			OwnerCharacter->AddInstanceComponent(Sphere);*/
		}

		if ( ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(OwnerCharacter) )
		{
			CSCharacter->NetMulticastMakeGravityCoreSphere(GravityCore->GetMeshRadius(), GravityCore->GetActorScale3D().X);
		}
	}

	DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, DurationTime);
	if ( DelayTask )
	{
		DelayTask->OnFinish.AddDynamic(this, &UCSGA_GravityCore::OnDurationFinished);
		DelayTask->ReadyForActivation();
	}
}

void UCSGA_GravityCore::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(OwnerCharacter))
	{
		CSCharacter->NetMulticastDestroyGravityCoreSphere();
	}

	if ( Sphere )
	{
		Sphere->DestroyComponent();
		Sphere = nullptr;
	}

	if ( DelayTask )
	{
		DelayTask->EndTask();
	}

	if (GravityCore)
	{
		GravityCore = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_GravityCore::OnDurationFinished()
{
	if ( GravityCore )
	{
		GravityCore->Destroy();
		GravityCore = nullptr;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
