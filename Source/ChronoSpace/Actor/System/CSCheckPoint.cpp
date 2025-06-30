// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSKillZone.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Game/CSGameMode.h"
#include "Components/BoxComponent.h"
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
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* Player = Cast<APawn>(OtherActor);
    if (Player && Player->IsPlayerControlled())
    {
        // Send RespawnPoint location to GameMode
        ACSGameMode* GameMode = GetCSGameMode();
        if (GameMode && ConnectedRespawnPoint)
        {
            GameMode->SetCurrentRespawnPoint(ConnectedRespawnPoint);
        }
    }
}

ACSGameMode* ACSCheckPoint::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}