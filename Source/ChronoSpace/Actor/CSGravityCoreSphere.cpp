// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSGravityCoreSphere.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Physics/CSCollision.h"
#include "Components/SphereComponent.h"
#include "ChronoSpace.h"


ACSGravityCoreSphere::ACSGravityCoreSphere()
{
	bReplicates = true;

	// GravitySphereTrigger
	SphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GravitySphereTrigger"));
	SphereTrigger->SetSphereRadius(GravityInfluenceRange, true);
	SphereTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	SphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	RootComponent = SphereTrigger;
	SphereTrigger->SetIsReplicated(true);

	// Static Mesh
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	StaticMeshComp->SetupAttachment(SphereTrigger); 
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComp->SetIsReplicated(true);

	MeshRadius = 50.0f;
}

void ACSGravityCoreSphere::BeginPlay()
{
	Super::BeginPlay();

	if ( !StaticMesh.IsValid() )
	{
		StaticMesh.LoadSynchronous();
	}
	
	if ( StaticMesh.IsValid() )
	{
		StaticMeshComp->SetStaticMesh(StaticMesh.Get());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("StaticMesh failed to load in ACSGravityCoreSphere"));
	}

	UE_LOG(LogCS, Log, TEXT("ACSGravityCoreSphere BeginPlay: %s"), *Owner.GetName());
}
