// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_GravityCore.h"
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

	if (GravityCore == nullptr)
	{
		OnCore();
	}
	else
	{
		OffCore();
	}
}

void UCSGA_GravityCore::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_GravityCore::OnCore()
{
	OwnerCharacter = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		FVector SpawnLocation = OwnerCharacter->GetActorLocation();

		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnTransform.SetRotation(FQuat::Identity);
		SpawnTransform.SetScale3D(FVector(CoreScale));

		GravityCore = GetWorld()->SpawnActorDeferred<ACSGravityCoreSphere>(GravityCoreClass, SpawnTransform, OwnerCharacter, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (GravityCore)
		{
			GravityCore->SetCheckComponentInMesh(bCheckMeshComponentAffectedByGravityCore);
			UGameplayStatics::FinishSpawningActor(GravityCore, SpawnTransform);
			GravityCore->AttachToActor(CurrentActorInfo->AvatarActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}

		if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(OwnerCharacter))
		{
			CSCharacter->NetMulticastMakeGravityCoreSphere(GravityCore->GetMeshRadius(), GravityCore->GetActorScale3D().X);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCSGA_GravityCore::OffCore()
{
	if (GravityCore)
	{
		GravityCore->Destroy();
		GravityCore = nullptr;
	}

	if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(OwnerCharacter))
	{
		CSCharacter->NetMulticastDestroyGravityCoreSphere();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}