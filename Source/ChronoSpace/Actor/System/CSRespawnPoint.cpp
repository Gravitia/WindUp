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