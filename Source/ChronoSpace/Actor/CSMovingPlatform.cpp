// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMovingPlatform.h"
#include "Actor/CSTrackManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"

ACSMovingPlatform::ACSMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// Platform movement is computed locally from Track, so no need to replicate movement
	bReplicates = false;
	SetReplicateMovement(false);
}

void ACSMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void ACSMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Track)
	{
		return;
	}

	USplineComponent* Spline = Track->GetSpline();
	const float SplineLength = Track->GetSplineLength();

	UE_LOG(LogTemp, Log, TEXT("SplineLength : %f"), SplineLength);


	if (!Spline || SplineLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float D = Track->GetSmoothedDistance() + OffsetDistance;

	D = FMath::Fmod(D, SplineLength);
	if (D < 0.f)
	{
		D += SplineLength;
	}

	const FTransform T = Spline->GetTransformAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);

	FVector Loc = T.GetLocation();
	Loc.Z += ZOffset;

	SetActorLocationAndRotation(Loc, T.GetRotation(), false, nullptr, ETeleportType::None);
}
