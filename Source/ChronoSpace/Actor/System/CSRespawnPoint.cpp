// Fill out your copyright notice in the Description page of Project Settings.


#include "CSRespawnPoint.h"
#include "Game/CSGameMode.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

ACSRespawnPoint::ACSRespawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // Direction Arrow as root
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    RootComponent = DirectionArrow;
    DirectionArrow->SetArrowColor(FLinearColor::Blue);
    DirectionArrow->ArrowSize = 2.0f;

    // Optional visual mesh
    RespawnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RespawnMesh"));
    RespawnMesh->SetupAttachment(RootComponent);
    RespawnMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RespawnMesh->SetVisibility(false); // Hidden by default
}

void ACSRespawnPoint::BeginPlay()
{
    Super::BeginPlay();

    GenerateID();

    // Register with GameMode
    ACSGameMode* GameMode = GetCSGameMode();
    if (GameMode)
    {
        GameMode->RegisterRespawnPoint(this);
    }
}

bool ACSRespawnPoint::SpawnPlayerHere(APawn* Player)
{
    if (!Player || !CanSpawnHere())
        return false;

    // Set player location and rotation
    Player->SetActorLocation(GetActorLocation());
    Player->SetActorRotation(GetActorRotation());

    // Clear velocity
    if (ACharacter* Character = Cast<ACharacter>(Player))
    {
        if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
        {
            MovementComp->Velocity = FVector::ZeroVector;
        }
    }

    OnPlayerSpawned(Player);

    UE_LOG(LogTemp, Log, TEXT("Player spawned at RespawnPoint: %s"), *RespawnPointID);
    return true;
}

bool ACSRespawnPoint::CanSpawnHere() const
{
    return bIsActive;
}

void ACSRespawnPoint::SetActive(bool bNewActive)
{
    if (bIsActive != bNewActive)
    {
        bIsActive = bNewActive;

        if (bNewActive)
        {
            OnRespawnPointActivated();
        }
        else
        {
            OnRespawnPointDeactivated();
        }
    }
}

void ACSRespawnPoint::GenerateID()
{
    FString SlotSuffix;
    switch (PlayerSlot)
    {
    case ERespawnPlayerSlot::Player1:
        SlotSuffix = "P1";
        break;
    case ERespawnPlayerSlot::Player2:
        SlotSuffix = "P2";
        break;
    default:
        SlotSuffix = "Any";
        break;
    }

    if (!ConnectedCheckPointID.IsEmpty())
    {
        RespawnPointID = FString::Printf(TEXT("RP_%s_%s_%02d"), *ConnectedCheckPointID, *SlotSuffix, Priority);
    }
    else
    {
        RespawnPointID = FString::Printf(TEXT("RP_Generic_%s_%d"), *SlotSuffix, FMath::RandRange(1000, 9999));
    }
}

int32 ACSRespawnPoint::GetPlayerSlotNumber(APawn* Player) const
{
    if (!Player || !Player->GetPlayerState())
        return 0;

    return Player->GetPlayerState()->GetPlayerId() + 1;
}

ACSGameMode* ACSRespawnPoint::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}
