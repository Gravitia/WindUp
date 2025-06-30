// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/CSGameMode.h"
#include "Game/CSGameState.h"
#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

ACSGameMode::ACSGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

    // Set our custom GameState
    GameStateClass = ACSGameState::StaticClass();

    CurrentRespawnPoint = nullptr;
}

void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("CSGameMode: Simple respawn system initialized"));
}

void ACSGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (NewPlayer && NewPlayer->GetPawn())
    {
        // Add player to GameState death tracking
        ACSGameState* CSGameState = GetCSGameState();
        if (CSGameState)
        {
            CSGameState->AddPlayerToDeathTracking(NewPlayer->GetPawn());
        }

        UE_LOG(LogTemp, Log, TEXT("Player logged in: %s"), *NewPlayer->GetName());
    }
}

void ACSGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        if (APawn* ExitingPawn = PC->GetPawn())
        {
            // Remove player from GameState death tracking
            ACSGameState* CSGameState = GetCSGameState();
            if (CSGameState)
            {
                CSGameState->RemovePlayerFromDeathTracking(ExitingPawn);
            }

            UE_LOG(LogTemp, Log, TEXT("Player logged out"));
        }
    }

    Super::Logout(Exiting);
}

void ACSGameMode::SetCurrentRespawnPoint(ACSRespawnPoint* NewRespawnPoint)
{
    if (CurrentRespawnPoint != NewRespawnPoint)
    {
        CurrentRespawnPoint = NewRespawnPoint;
        OnRespawnPointChanged(NewRespawnPoint);

        UE_LOG(LogTemp, Log, TEXT("Current respawn point updated"));
    }
}

void ACSGameMode::RespawnAllPlayersAtCurrentPoint()
{
    if (!CurrentRespawnPoint)
    {
        UE_LOG(LogTemp, Warning, TEXT("No current respawn point set"));
        return;
    }

    // Get all players from GameState
    ACSGameState* CSGameState = GetCSGameState();
    if (!CSGameState)
    {
        UE_LOG(LogTemp, Warning, TEXT("CSGameState not found for respawn"));
        return;
    }

    // Get all players (both dead and alive)
    TArray<APawn*> AllPlayers;
    AllPlayers.Append(CSGameState->GetDeadPlayers());
    AllPlayers.Append(CSGameState->GetAlivePlayers());

    // Respawn all players at current respawn point
    for (APawn* Player : AllPlayers)
    {
        if (Player && CurrentRespawnPoint)
        {
            CurrentRespawnPoint->SpawnPlayerHere(Player);
        }
    }

    // Reset all death states in GameState
    for (APawn* Player : AllPlayers)
    {
        if (Player)
        {
            CSGameState->HandlePlayerRevive(Player);
        }
    }

    OnAllPlayersRespawned();

    UE_LOG(LogTemp, Log, TEXT("All players respawned at current respawn point"));
}

void ACSGameMode::HandlePlayerDeath(APawn* DeadPlayer)
{
    // Delegate to GameState for state management
    ACSGameState* CSGameState = GetCSGameState();
    if (CSGameState)
    {
        CSGameState->HandlePlayerDeath(DeadPlayer);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CSGameState not found for player death handling"));
    }
}

ACSGameState* ACSGameMode::GetCSGameState() const
{
    return Cast<ACSGameState>(GameState);
}