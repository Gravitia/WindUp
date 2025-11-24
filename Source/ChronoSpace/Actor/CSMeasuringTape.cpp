// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMeasuringTape.h"

ACSMeasuringTape::ACSMeasuringTape()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainMesh"));
    RootComponent = Mesh;

    // Eyes
    EyesMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyesMesh"));
    EyesMesh->SetupAttachment(RootComponent);

    // Nose
    NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
    NoseMesh->SetupAttachment(RootComponent);

    // Button
    ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
    ButtonMesh->SetupAttachment(RootComponent);

    // Default Ruler
    RulerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RulerMesh"));
    RulerMesh->SetupAttachment(RootComponent);

}

// Called when the game starts or when spawned
void ACSMeasuringTape::BeginPlay()
{
	Super::BeginPlay();
	
}

