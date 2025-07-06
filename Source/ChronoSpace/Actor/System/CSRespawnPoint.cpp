// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSRespawnPoint.h"
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
}

void ACSRespawnPoint::SpawnPlayerHere(APawn* Player)
{
    if (!Player)
        return;

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

    UE_LOG(LogTemp, Log, TEXT("Player spawned at RespawnPoint"));
}