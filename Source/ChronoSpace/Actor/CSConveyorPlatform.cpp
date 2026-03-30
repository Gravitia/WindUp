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

	SetReplicateMovement(false);
	bReplicates = true;


}

void ACSConveyorPlatform::SetManager(ACSConveyorManager* InManager)
{
	Manager = InManager;
}

void ACSConveyorPlatform::SetIndexOffset(float InOffsetDistance)
{
	OffsetDistance = InOffsetDistance;
}

void ACSConveyorPlatform::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Manager)
    {
        return;
    }

    const float TotalLength = Manager->GetTotalLength();
    if (TotalLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    float D = OffsetDistance + Manager->GetSmoothedProgress();
    D = FMath::Fmod(D, TotalLength);
    if (D < 0.f)
    {
        D += TotalLength;
    }

    // Use manager's transform to calculate world position and rotation
    FVector LocalOffset(0.f, D, ZOffset);
    FTransform TargetTransform = Manager->GetActorTransform();
    
    // Position: Manager Location + (Manager Rotation * LocalOffset)
    FVector WorldLoc = TargetTransform.TransformPosition(LocalOffset);
    
    // Update both location and rotation
    SetActorLocationAndRotation(WorldLoc, TargetTransform.GetRotation(), false, nullptr, ETeleportType::None);
}
