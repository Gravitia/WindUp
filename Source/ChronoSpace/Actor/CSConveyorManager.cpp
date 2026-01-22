// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorManager.h"
#include "Actor/CSConveyorPlatform.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ACSConveyorManager::ACSConveyorManager()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;

	BeltISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BeltISM"));
	BeltISM->SetupAttachment(RootComponent);
	BeltISM->SetMobility(EComponentMobility::Movable);
	BeltISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACSConveyorManager::BeginPlay()
{
	Super::BeginPlay();

	if (Spline)
	{
		SplineLength = Spline->GetSplineLength();
	}

	TargetDistance = RepDistance;
	SmoothedDistance = RepDistance;

	// ½ÇÁ¦ ¹Ù´Ú
	BuildPlatforms();
}

void ACSConveyorManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Spline || SplineLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Server: authoritative progress
	if (HasAuthority())
	{
		RepDistance += MoveSpeed * DeltaSeconds;
		RepDistance = FMath::Fmod(RepDistance, SplineLength);

		if (RepDistance < 0.f)
		{
			RepDistance += SplineLength;
		}

		TargetDistance = RepDistance;
	}

	// Client-side smoothing
	const float Delta = FMath::Abs(TargetDistance - SmoothedDistance);

	if (Delta > SplineLength * 0.5f)
	{
		SmoothedDistance = TargetDistance;
	}
	else
	{
		SmoothedDistance =
			FMath::FInterpTo(SmoothedDistance, TargetDistance, DeltaSeconds, InterpSpeed);
	}
}

void ACSConveyorManager::RebuildConveyor()
{
	if (!Spline)
	{
		return;
	}

	SplineLength = Spline->GetSplineLength();

	// ±âÁ¸ ÇÃ·§Æû Á¦°Å
	for (ACSConveyorPlatform* Platform : Platforms)
	{
		if (Platform)
		{
			Platform->Destroy();
		}
	}
	Platforms.Empty();

	// ½Ã°¢ ¸Þ½¬
	BuildBeltMeshes();

}

void ACSConveyorManager::BuildPlatforms()
{
	if (!ConveyorPlatformClass || !Spline)
	{
		return;
	}

	// ±âÁ¸ ÇÃ·§Æû Á¦°Å
	for (ACSConveyorPlatform* Platform : Platforms)
	{
		if (Platform)
		{
			Platform->Destroy();
		}
	}
	Platforms.Empty();

	float AccumulatedDistance = 0.f;

	while (AccumulatedDistance < SplineLength)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACSConveyorPlatform* Platform =
			GetWorld()->SpawnActor<ACSConveyorPlatform>(
				ConveyorPlatformClass,
				FTransform::Identity,
				Params
			);

		if (!Platform)
		{
			break;
		}

		const float PlatformLength = Platform->GetMeshLength();

		if (PlatformLength <= KINDA_SMALL_NUMBER)
		{
			Platform->Destroy();
			break;
		}

		Platform->Init(this, AccumulatedDistance);
		Platforms.Add(Platform);

		AccumulatedDistance += PlatformLength;
	}
}


void ACSConveyorManager::OnRep_RepDistance()
{
	TargetDistance = RepDistance;
}

void ACSConveyorManager::BuildBeltMeshes()
{
	if (!Spline || !BeltMesh || !BeltISM)
	{
		return;
	}

	BeltISM->ClearInstances();
	BeltISM->SetStaticMesh(BeltMesh);

	const FBoxSphereBounds Bounds = BeltMesh->GetBounds();
	const float MeshLength = Bounds.BoxExtent.X * 2.f * MeshScale.X;
	const float Step = (MeshSpacing > 0.f) ? MeshSpacing : MeshLength;

	if (Step <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 Count = FMath::FloorToInt(SplineLength / Step);

	for (int32 i = 0; i < Count; ++i)
	{
		const float Distance = i * Step;

		const FVector Location =
			Spline->GetLocationAtDistanceAlongSpline(
				Distance,
				ESplineCoordinateSpace::Local
			);

		const FRotator Rotation =
			Spline->GetRotationAtDistanceAlongSpline(
				Distance,
				ESplineCoordinateSpace::Local
			);

		FTransform Transform;
		Transform.SetLocation(Location);
		Transform.SetRotation(Rotation.Quaternion());
		Transform.SetScale3D(MeshScale);

		BeltISM->AddInstance(Transform);
	}
}

void ACSConveyorManager::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSConveyorManager, RepDistance);
}