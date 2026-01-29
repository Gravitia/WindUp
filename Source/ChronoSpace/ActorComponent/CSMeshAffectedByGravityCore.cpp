// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Physics/CSCollision.h"

UCSMeshAffectedByGravityCore::UCSMeshAffectedByGravityCore()
{
	SetIsReplicated(true);
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
}

