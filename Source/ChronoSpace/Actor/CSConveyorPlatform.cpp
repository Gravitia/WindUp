// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorPlatform.h"
#include "Actor/CSConveyorManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"

ACSConveyorPlatform::ACSConveyorPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// 컨베이어 바닥은 서버에서만 위치 계산
	bReplicates = false;
	SetReplicateMovement(false);

	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ACSConveyorPlatform::BeginPlay()
{
	Super::BeginPlay();
}

float ACSConveyorPlatform::GetMeshLength() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return 0.f;
	}

	const FBox BoundingBox = Mesh->GetStaticMesh()->GetBoundingBox();

	// FBox는 Min / Max 멤버를 직접 사용
	const float LocalLength = BoundingBox.Max.X - BoundingBox.Min.X;

	// 스케일 반영
	return FMath::Abs(LocalLength * Mesh->GetComponentScale().X);
}

void ACSConveyorPlatform::Init(ACSConveyorManager* InManager, float InOffsetDistance)
{
	Manager = InManager;
	OffsetDistance = InOffsetDistance;

	// 시각적으로는 숨기되, 충돌은 유지
	SetActorHiddenInGame(true);
	SetActorEnableCollision(true);
}

void ACSConveyorPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Manager)
	{
		return;
	}

	USplineComponent* Spline = Manager->GetSpline();
	const float SplineLength = Manager->GetSplineLength();

	if (!Spline || SplineLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float Distance = Manager->GetSmoothedDistance() + OffsetDistance;
	Distance = FMath::Fmod(Distance, SplineLength);

	if (Distance < 0.f)
	{
		Distance += SplineLength;
	}

	const FTransform T =
		Spline->GetTransformAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World
		);

	FVector Location = T.GetLocation();
	Location.Z += ZOffset;

	SetActorLocationAndRotation(
		Location,
		T.GetRotation(),
		false,
		nullptr,
		ETeleportType::None
	);
}