// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"

UCSMeshAffectedByGravityCore::UCSMeshAffectedByGravityCore()
{
	// SetIsReplicated(true);

	SetIsReplicatedByDefault(true);
}

void UCSMeshAffectedByGravityCore::BeginPlay()
{
	Super::BeginPlay(); 

	AActor* Owner = GetOwner(); 
	if (!Owner) return; 

	UStaticMeshComponent* MeshComp = 
		Owner->FindComponentByClass<UStaticMeshComponent>(); 

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("No StaticMeshComponent found on %s"), *Owner->GetName()); 
		return;
	}
	MeshComp->SetCollisionObjectType(CCHANNEL_CSGRAVITY_CORE_AFFECTED);
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void UCSMeshAffectedByGravityCore::NotifyInteractionStarted()
{
	OnInteractionStarted.Broadcast();
}

void UCSMeshAffectedByGravityCore::NotifyInteractionEnded()
{
	OnInteractionEnded.Broadcast();
}

void UCSMeshAffectedByGravityCore::SetEnable( bool bInEnable )
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UStaticMeshComponent* MeshComp = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (!IsValid(MeshComp)) return;

	bEnable = bInEnable;

	MeshComp->SetCollisionObjectType(
		bEnable ? CCHANNEL_CSGRAVITY_CORE_AFFECTED : ECC_WorldDynamic
	);

	MeshComp->SetGenerateOverlapEvents(true);
	MeshComp->UpdateOverlaps();
}

