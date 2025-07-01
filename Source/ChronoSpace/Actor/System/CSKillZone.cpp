// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSKillZone.h"
#include "Game/CSGameState.h"
#include "Game/CSGameMode.h"
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

    // 수정: OnTriggerBeginOverlap으로 바인딩 변경
    KillVolume->OnComponentBeginOverlap.AddDynamic(this, &ACSKillZone::OnTriggerBeginOverlap);
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

    UE_LOG(LogTemp, Warning, TEXT("Player killed by KillZone: %s"), *Player->GetName());
}

void ACSKillZone::SetActive(bool bNewActive)
{
    bIsActive = bNewActive;
}

// 수정: 함수명을 OnTriggerBeginOverlap으로 변경
void ACSKillZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Log, TEXT("CSLog : KillZone OnTriggerBeginOverlap"));

    if (!bIsActive)
        return;

    APawn* Player = Cast<APawn>(OtherActor);
    if (Player && Player->IsPlayerControlled())
    {
        // 즉시 리스폰
        ACSGameMode* GameMode = GetCSGameMode();
        if (GameMode)
        {
            GameMode->RespawnSinglePlayer(Player);
            UE_LOG(LogTemp, Log, TEXT("Player instantly respawned: %s"), *Player->GetName());
        }
    }
}

void ACSKillZone::KillPlayerWithDelay(APawn* Player, float DelayTime)
{
    if (!Player || !IsValid(Player))
        return;

    // 딜레이 후 리스폰
    FTimerHandle RespawnTimer;
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer,
        [this, Player]()
        {
            ACSGameMode* GameMode = GetCSGameMode();
            if (GameMode && IsValid(Player))
            {
                GameMode->RespawnSinglePlayer(Player);
            }
        },
        DelayTime,
        false);
}

ACSGameState* ACSKillZone::GetCSGameState() const
{
    return Cast<ACSGameState>(GetWorld()->GetGameState());
}

ACSGameMode* ACSKillZone::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}