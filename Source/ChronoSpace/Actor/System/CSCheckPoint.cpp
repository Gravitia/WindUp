// Fill out your copyright notice in the Description page of Project Settings.


#include "CSCheckPoint.h"
#include "Game/CSGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

ACSCheckPoint::ACSCheckPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // TriggerBox as root
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionProfileName("Trigger");

    // Optional visual mesh
    CheckPointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CheckPointMesh"));
    CheckPointMesh->SetupAttachment(RootComponent);
    CheckPointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    GenerateID();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);

    // Register with GameMode
    ACSGameMode* GameMode = GetCSGameMode();
    if (GameMode)
    {
        GameMode->RegisterCheckpoint(this);
    }
}

void ACSCheckPoint::ActivateCheckPoint()
{
    if (CurrentState != ECheckPointState::Active)
    {
        CurrentState = ECheckPointState::Active;
        OnCheckPointActivated();

        UE_LOG(LogTemp, Log, TEXT("CheckPoint activated: %s"), *CheckPointID);
    }
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* Player = Cast<APawn>(OtherActor);
    if (Player && Player->IsPlayerControlled())
    {
        // Tell GameMode to activate this checkpoint
        ACSGameMode* GameMode = GetCSGameMode();
        if (GameMode)
        {
            GameMode->ActivateCheckpoint(CheckPointID);
        }
    }
}

void ACSCheckPoint::GenerateID()
{
    CheckPointID = FString::Printf(TEXT("C%d_S%d_CP%02d"), ChapterNumber, StageNumber, CheckPointNumber);
}

ACSGameMode* ACSCheckPoint::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}

