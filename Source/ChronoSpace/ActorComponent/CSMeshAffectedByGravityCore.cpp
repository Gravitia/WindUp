// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Components/StaticMeshComponent.h"
#include "Physics/CSCollision.h"
#include "Net/UnrealNetwork.h"
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

	Owner->SetReplicates(true);
	Owner->SetReplicateMovement(true);
	MeshComp->SetIsReplicated(true);

	if (!Owner->HasAuthority())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetEnableGravity(false);
	}

	MeshComp->SetCollisionObjectType(CCHANNEL_CSGRAVITY_CORE_AFFECTED);
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void UCSMeshAffectedByGravityCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCSMeshAffectedByGravityCore, InfluenceCount);
}

void UCSMeshAffectedByGravityCore::AddInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	++InfluenceCount;
	OnRep_InfluenceCount();	// 리슨 호스트는 OnRep 을 받지 않는다
}

void UCSMeshAffectedByGravityCore::RemoveInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	if (InfluenceCount <= 0) return;

	--InfluenceCount;
	OnRep_InfluenceCount();
}

void UCSMeshAffectedByGravityCore::OnRep_InfluenceCount()
{
	const bool bNowActive = (InfluenceCount > 0);
	if (bNowActive == bBroadcastActive) return;

	bBroadcastActive = bNowActive;

	if (bNowActive)
	{
		OnInteractionStarted.Broadcast();
	}
	else
	{
		OnInteractionEnded.Broadcast();
	}
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

