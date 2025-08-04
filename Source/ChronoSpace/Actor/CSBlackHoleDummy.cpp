// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSBlackHoleDummy.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
ACSBlackHoleDummy::ACSBlackHoleDummy()
{
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	RootComponent = Core;

	Field = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Field"));
	Field->SetupAttachment(RootComponent);
}

void ACSBlackHoleDummy::SetGravityInfluenceRange(float Range)
{
	if ( Field )
	{
		Field->SetRelativeScale3D(FVector( Range / MeshRadius ));
	}
}




