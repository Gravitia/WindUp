// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/System/CSKillZone.h"
#include "Game/CSGameState.h"
#include "Game/CSGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Character/CSCharacterPlayer.h"
#include "Common/CSCommon.h"
#include "ChronoSpace.h"

ACSKillZone::ACSKillZone()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;

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

    // Trigger 바인딩
    KillVolume->OnComponentBeginOverlap.AddDynamic(
        this,
        &ACSKillZone::OnTriggerBeginOverlap
    );

    VisualMesh->SetVisibility(bShowVisualMesh);
}

void ACSKillZone::KillPlayer(APawn* Player)
{
    if (!Player || !IsValid(Player))
        return;

    // GameState에 사망 알림
    if (ACSGameState* GameState = GetCSGameState())
    {
        GameState->HandlePlayerDeath(Player);
    }

    // 캐릭터 사망 처리
    if (ACSCharacterPlayer* CharacterPlayer = Cast<ACSCharacterPlayer>(Player))
    {
        CharacterPlayer->SetDead();
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Player killed by KillZone: %s"),
        *Player->GetName()
    );
}

void ACSKillZone::SetActive(bool bNewActive)
{
    bIsActive = bNewActive;
}

void ACSKillZone::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    UE_LOG(LogTemp, Log, TEXT("CSLog : KillZone OnTriggerBeginOverlap"));

    if (!bIsActive)
        return;

    ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
    if (Player && Player->IsPlayerControlled() && !Player->bIgnoreKillZone)
    {
        KillPlayer(Player);
        RevivePlayerWithDelay(Player, Player->GetReviveTime());
    }
}

void ACSKillZone::RevivePlayerWithDelay(APawn* Player, float DelayTime)
{
    if (!Player || !IsValid(Player))
        return;

    FTimerHandle RespawnTimer;
    TWeakObjectPtr<ACSKillZone> WeakThis = this;

    if (ACSCharacterPlayer* CharacterPlayer = Cast<ACSCharacterPlayer>(Player))
    {
        TWeakObjectPtr<ACSCharacterPlayer> WeakPlayer = CharacterPlayer;

        GetWorld()->GetTimerManager().SetTimer(
            RespawnTimer,
            [WeakThis, WeakPlayer]()
            {
                if (!WeakThis.IsValid() || !WeakPlayer.IsValid())
                    return;
                
                // SetRevive를 RespawnSinglePlayer 보다 먼저 호출해야함. 
                WeakPlayer->SetRevive();

                if (ACSGameMode* GameMode = WeakThis->GetCSGameMode())
                {
                    GameMode->RespawnSinglePlayer(WeakPlayer.Get());
                }

            },
            DelayTime,
            false
        );
    }
}

ACSGameState* ACSKillZone::GetCSGameState() const
{
    return Cast<ACSGameState>(GetWorld()->GetGameState());
}

ACSGameMode* ACSKillZone::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}
