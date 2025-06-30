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
}

void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    InitializeLevelActors();
    BuildCheckpointRespawnMap();

    // Set default active checkpoint
    if (LevelCheckpoints.Num() > 0)
    {
        ActivateCheckpoint("C1_S1_CP01"); // Default first checkpoint
    }
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
            CSGameState->AddPlayerToTracking(NewPlayer->GetPawn());
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
                CSGameState->RemovePlayerFromTracking(ExitingPawn);
            }

            UE_LOG(LogTemp, Log, TEXT("Player logged out"));
        }
    }

    Super::Logout(Exiting);
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

void ACSGameMode::RespawnAllPlayersAtCheckpoint()
{
    if (CurrentActiveCheckpointID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active checkpoint for respawn"));
        return;
    }

    TArray<ACSRespawnPoint*> AvailableRespawnPoints = GetRespawnPointsForCheckpoint(CurrentActiveCheckpointID);

    if (AvailableRespawnPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No respawn points found for checkpoint: %s"), *CurrentActiveCheckpointID);
        return;
    }

    // Sort respawn points by priority (highest first)
    AvailableRespawnPoints.Sort([](const ACSRespawnPoint& A, const ACSRespawnPoint& B) {
        return A.Priority > B.Priority;
        });

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

    int32 RespawnIndex = 0;

    // Respawn all players
    for (APawn* Player : AllPlayers)
    {
        if (Player)
        {
            ACSRespawnPoint* RespawnPoint = AvailableRespawnPoints[RespawnIndex % AvailableRespawnPoints.Num()];
            if (RespawnPoint)
            {
                SpawnPlayerAtRespawnPoint(Player, RespawnPoint);
                RespawnIndex++;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("All players respawned at checkpoint: %s"), *CurrentActiveCheckpointID);
}

void ACSGameMode::ActivateCheckpoint(const FString& CheckpointID)
{
    // Deactivate all checkpoints first
    DeactivateAllCheckpoints();

    // Find and activate target checkpoint
    ACSCheckPoint* TargetCheckpoint = FindCheckpointByID(CheckpointID);
    if (TargetCheckpoint)
    {
        CurrentActiveCheckpointID = CheckpointID;
        TargetCheckpoint->ActivateCheckPoint();

        // Activate connected respawn points
        TArray<ACSRespawnPoint*>* ConnectedRespawnPoints = CheckpointRespawnMap.Find(CheckpointID);
        if (ConnectedRespawnPoints)
        {
            for (ACSRespawnPoint* RespawnPoint : *ConnectedRespawnPoints)
            {
                if (RespawnPoint)
                {
                    RespawnPoint->SetActive(true);
                }
            }
        }

        UE_LOG(LogTemp, Log, TEXT("Checkpoint activated: %s"), *CheckpointID);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Checkpoint not found: %s"), *CheckpointID);
    }
}

ACSCheckPoint* ACSGameMode::FindCheckpointByID(const FString& CheckpointID) const
{
    for (ACSCheckPoint* Checkpoint : LevelCheckpoints)
    {
        if (Checkpoint && Checkpoint->GetCheckPointID() == CheckpointID)
        {
            return Checkpoint;
        }
    }
    return nullptr;
}

void ACSGameMode::RegisterCheckpoint(ACSCheckPoint* Checkpoint)
{
    if (Checkpoint && !LevelCheckpoints.Contains(Checkpoint))
    {
        LevelCheckpoints.Add(Checkpoint);
        UE_LOG(LogTemp, Log, TEXT("Checkpoint registered: %s"), *Checkpoint->GetCheckPointID());
    }
}

void ACSGameMode::RegisterRespawnPoint(ACSRespawnPoint* RespawnPoint)
{
    if (RespawnPoint && !LevelRespawnPoints.Contains(RespawnPoint))
    {
        LevelRespawnPoints.Add(RespawnPoint);
        UE_LOG(LogTemp, Log, TEXT("RespawnPoint registered: %s"), *RespawnPoint->GetRespawnPointID());
    }
}

ACSRespawnPoint* ACSGameMode::GetBestRespawnPoint(const FString& CheckpointID) const
{
    TArray<ACSRespawnPoint*> ConnectedPoints = GetRespawnPointsForCheckpoint(CheckpointID);

    if (ConnectedPoints.Num() == 0)
        return nullptr;

    // Sort by priority (highest first)
    ConnectedPoints.Sort([](const ACSRespawnPoint& A, const ACSRespawnPoint& B) {
        return A.Priority > B.Priority;
        });

    // Find first available spawn point
    for (ACSRespawnPoint* RespawnPoint : ConnectedPoints)
    {
        if (RespawnPoint && RespawnPoint->CanSpawnHere())
        {
            return RespawnPoint;
        }
    }

    // Fallback to first point if no ideal point found
    return ConnectedPoints.Num() > 0 ? ConnectedPoints[0] : nullptr;
}

TArray<ACSRespawnPoint*> ACSGameMode::GetRespawnPointsForCheckpoint(const FString& CheckpointID) const
{
    const TArray<ACSRespawnPoint*>* ConnectedPoints = CheckpointRespawnMap.Find(CheckpointID);
    return ConnectedPoints ? *ConnectedPoints : TArray<ACSRespawnPoint*>();
}

void ACSGameMode::InitializeLevelActors()
{
    LevelCheckpoints.Empty();
    LevelRespawnPoints.Empty();

    // Find all checkpoints in level
    for (TActorIterator<ACSCheckPoint> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        ACSCheckPoint* Checkpoint = *ActorItr;
        RegisterCheckpoint(Checkpoint);
    }

    // Find all respawn points in level
    for (TActorIterator<ACSRespawnPoint> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        ACSRespawnPoint* RespawnPoint = *ActorItr;
        RegisterRespawnPoint(RespawnPoint);
    }

    UE_LOG(LogTemp, Log, TEXT("Found %d checkpoints and %d respawn points"),
        LevelCheckpoints.Num(), LevelRespawnPoints.Num());
}

void ACSGameMode::BuildCheckpointRespawnMap()
{
    CheckpointRespawnMap.Empty();

    // Build mapping between checkpoints and respawn points
    for (ACSRespawnPoint* RespawnPoint : LevelRespawnPoints)
    {
        if (RespawnPoint && !RespawnPoint->ConnectedCheckPointID.IsEmpty())
        {
            FString CheckpointID = RespawnPoint->ConnectedCheckPointID;

            if (!CheckpointRespawnMap.Contains(CheckpointID))
            {
                CheckpointRespawnMap.Add(CheckpointID, TArray<ACSRespawnPoint*>());
            }

            CheckpointRespawnMap[CheckpointID].Add(RespawnPoint);
        }
    }

    // Log mapping results
    for (const auto& Pair : CheckpointRespawnMap)
    {
        UE_LOG(LogTemp, Log, TEXT("Checkpoint %s has %d respawn points"),
            *Pair.Key, Pair.Value.Num());
    }

    UE_LOG(LogTemp, Log, TEXT("Built checkpoint-respawn mapping for %d checkpoints"),
        CheckpointRespawnMap.Num());
}

void ACSGameMode::SpawnPlayerAtRespawnPoint(APawn* Player, ACSRespawnPoint* RespawnPoint)
{
    if (Player && RespawnPoint)
    {
        bool bSpawnSuccess = RespawnPoint->SpawnPlayerHere(Player);
        if (bSpawnSuccess)
        {
            UE_LOG(LogTemp, Log, TEXT("Player spawned at %s"), *RespawnPoint->GetRespawnPointID());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to spawn player at %s"), *RespawnPoint->GetRespawnPointID());
        }
    }
}

void ACSGameMode::DeactivateAllCheckpoints()
{
    // Deactivate all checkpoints
    for (ACSCheckPoint* Checkpoint : LevelCheckpoints)
    {
        if (Checkpoint && Checkpoint->IsActive())
        {
            UE_LOG(LogTemp, Log, TEXT("Deactivating checkpoint: %s"), *Checkpoint->GetCheckPointID());
        }
    }

    // Deactivate all respawn points
    for (ACSRespawnPoint* RespawnPoint : LevelRespawnPoints)
    {
        if (RespawnPoint)
        {
            RespawnPoint->SetActive(false);
        }
    }

    CurrentActiveCheckpointID.Empty();
}

ACSGameState* ACSGameMode::GetCSGameState() const
{
    return Cast<ACSGameState>(GameState);
}

