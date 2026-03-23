// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSGravityCoreSphere.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Physics/CSCollision.h"
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
	SphereTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RootComponent = SphereTrigger;
	SphereTrigger->SetIsReplicated(true);

	SphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerBeginOverlap);
	SphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerEndOverlap);
	SphereTrigger->SetCollisionResponseToChannel(
		CCHANNEL_CSGRAVITY_CORE_AFFECTED,
		ECR_Overlap
	);
	SphereTrigger->SetCollisionResponseToChannel(
		CCHANNEL_CSGRAVITY_CORE,
		ECR_Ignore
	);

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
	
	if ( StaticMesh.IsValid() && IsValid( StaticMeshComp ) )
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
	if (!HasAuthority()) return;

	//ProcessForCharacter(DeltaSeconds);
	ProcessForStaticMesh(DeltaSeconds);
}

void ACSGravityCoreSphere::ProcessForStaticMesh(float DeltaTime)
{
	if( !IsValid(SphereTrigger) ) return;
	const FVector CoreLocation = SphereTrigger->GetComponentLocation();

	for (auto It = StaticMeshesInSphereTrigger.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->Get();

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

		/*float MeshMass = FMath::Max(Mesh->GetMass(), 100.0f);

		Power *= (MeshMass / 100);*/

		const FVector Direction = ToBH.GetSafeNormal();
		Mesh->AddImpulse(Direction * Power * 30 * DeltaTime, NAME_None, true);
	}
}

void ACSGravityCoreSphere::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	if (!HasAuthority()) return;

	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		if ( bCheckMeshHaveComponent && OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>() == nullptr ) return;

		TargetStaticMeshComp->SetPhysicsMaxAngularVelocityInDegrees(180.f);
		TargetStaticMeshComp->SetAngularDamping(2.0f);

		TargetStaticMeshComp->SetEnableGravity(false);
		StaticMeshesInSphereTrigger.Add(TargetStaticMeshComp);

		if (UCSMeshAffectedByGravityCore* Comp = OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>())
		{
			Comp->NotifyInteractionStarted();
		}
	}
}

void ACSGravityCoreSphere::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;

	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		if (bCheckMeshHaveComponent && OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>())

		TargetStaticMeshComp->SetEnableGravity(true);
		StaticMeshesInSphereTrigger.Remove(TargetStaticMeshComp);

		if (UCSMeshAffectedByGravityCore* Comp = OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>())
		{
			Comp->NotifyInteractionEnded();
		}
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
