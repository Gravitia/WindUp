// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSKillZone.h"
#include "Game/CSGameState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ACSKillZone::ACSKillZone()
{
    PrimaryActorTick.bCanEverTick = false;

    // Kill Volume as root
    KillVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("KillVolume"));
    RootComponent = KillVolume;
    KillVolume->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
    KillVolume->SetCollisionProfileName("Trigger");

    // Optional visual mesh
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(RootComponent);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetVisibility(bShowVisualMesh);
}

void ACSKillZone::BeginPlay()
{
    Super::BeginPlay();

    KillVolume->OnComponentBeginOverlap.AddDynamic(this, &ACSKillZone::OnVolumeBeginOverlap);
    VisualMesh->SetVisibility(bShowVisualMesh);
}

void ACSKillZone::KillPlayer(APawn* Player)
{
    if (!Player || !IsValid(Player))
        return;

    // Tell GameState that player died
    ACSGameState* GameState = GetCSGameState();
    if (GameState)
    {
        GameState->HandlePlayerDeath(Player);
    }

    OnPlayerKilled(Player);

    UE_LOG(LogTemp, Warning, TEXT("Player killed by KillZone: %s"), *Player->GetName());
}

void ACSKillZone::SetActive(bool bNewActive)
{
    bIsActive = bNewActive;
}

void ACSKillZone::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bIsActive)
        return;

    APawn* Player = Cast<APawn>(OtherActor);
    if (Player)
    {
        // Check if affects players only
        if (bAffectsPlayersOnly && !Player->IsPlayerControlled())
            return;

        KillPlayer(Player);
    }
}

ACSGameState* ACSKillZone::GetCSGameState() const
{
    return Cast<ACSGameState>(GetWorld()->GetGameState());
}