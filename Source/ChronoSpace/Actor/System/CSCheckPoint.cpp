// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCheckPoint.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
// #include "CSGame"

// Sets default values
ACSCheckPoint::ACSCheckPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACSCheckPoint::BeginPlay()
{
	Super::BeginPlay();
	
}

void ACSCheckPoint::UnlockCheckPoint()
{
}

void ACSCheckPoint::ActivateCheckPoint()
{
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}

void ACSCheckPoint::GenerateID()
{
}



