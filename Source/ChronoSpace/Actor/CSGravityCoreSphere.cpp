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
	PrimaryActorTick.bCanEverTick = true;

	// GravitySphereTrigger
	SphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GravitySphereTrigger"));
	SphereTrigger->SetSphereRadius(GravityInfluenceRange, true);
	SphereTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	SphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	RootComponent = SphereTrigger;
	SphereTrigger->SetIsReplicated(true);

	SphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerBeginOverlap);
	SphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerEndOverlap);

	// Static Mesh
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	StaticMeshComp->SetupAttachment(SphereTrigger); 
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComp->SetIsReplicated(true);
	StaticMeshComp->OnComponentBeginOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnCoreBeginOverlap);

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

void ACSGravityCoreSphere::Tick(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	//ProcessForCharacter(DeltaTime);
	ProcessForStaticMesh(DeltaSeconds);
}

void ACSGravityCoreSphere::ProcessForStaticMesh(float DeltaTime)
{
	const FVector CoreLocation = SphereTrigger->GetComponentLocation();

	for (auto It = StaticMeshesInSphereTrigger.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It.Value();

		if (!IsValid(Mesh))
		{
			It.RemoveCurrent();
			continue;
		}

		if (!Mesh->IsSimulatingPhysics())
		{
			continue;
		}

		FVector Power(1000.0f, 1000.0f, 1000.0f);
		const FVector MeshLocation = Mesh->GetComponentLocation();
		const FVector ToBH = CoreLocation - MeshLocation;
		const float Distance = ToBH.Size();

		const FVector Direction = ToBH.GetSafeNormal();
		//UE_LOG(LogCS, Log, TEXT("AddImpulse Mesh"));
		Mesh->AddImpulse(Direction * Power * 30 * DeltaTime, NAME_None, true);
	}
}

void ACSGravityCoreSphere::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		TargetStaticMeshComp->SetPhysicsMaxAngularVelocityInDegrees(180.f); 
		TargetStaticMeshComp->SetAngularDamping(2.0f);

		TargetStaticMeshComp->SetEnableGravity(false);
		StaticMeshesInSphereTrigger.Emplace(TargetStaticMeshComp->GetFName(), TargetStaticMeshComp);
	}
}

void ACSGravityCoreSphere::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		TargetStaticMeshComp->SetEnableGravity(true);
		StaticMeshesInSphereTrigger.Remove(TargetStaticMeshComp->GetFName());
	}
}

void ACSGravityCoreSphere::OnCoreBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		TargetStaticMeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
		TargetStaticMeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
	}
}
