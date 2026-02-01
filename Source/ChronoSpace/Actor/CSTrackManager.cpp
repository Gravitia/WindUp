// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSTrackManager.h"
#include "Components/SplineComponent.h"
#include "Net/UnrealNetwork.h"

ACSTrackManager::ACSTrackManager()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;

	NetUpdateFrequency = 60.f;     // ±âº»º¸´Ù ÈÎ¾À ÀÚÁÖ
	MinNetUpdateFrequency = 30.f;  // ¶³¾îÁú ¶§ ¹Ù´Ú°ª
}

void ACSTrackManager::BeginPlay()
{
	Super::BeginPlay();

	SplineLength = (Spline ? Spline->GetSplineLength() : 0.f);

	TargetDistance = RepDistance;
	SmoothedDistance = RepDistance;
}

void ACSTrackManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Spline || SplineLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Server: authoritatively advance RepDistance
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

	// Everyone: smooth toward target for stable visuals
	const float Delta = FMath::Abs(TargetDistance - SmoothedDistance);

	// If wrapped around (near end -> 0), snap instead of interpolating the long way
	if (Delta > SplineLength * 0.5f)
	{
		SmoothedDistance = TargetDistance;
	}
	else
	{
		SmoothedDistance = FMath::FInterpTo(SmoothedDistance, TargetDistance, DeltaSeconds, InterpSpeed);
	}
}

void ACSTrackManager::OnRep_RepDistance()
{
	TargetDistance = RepDistance;

	// Optional: if you want immediate correction when packet arrives
	// SmoothedDistance = TargetDistance;
}

void ACSTrackManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSTrackManager, RepDistance);
}
