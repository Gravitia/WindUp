// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Player/CSPlayerState.h"

ACSCheckPoint::ACSCheckPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionProfileName("Trigger");

    bReplicates = true;

    ConnectedRespawnPoint = nullptr;
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);

    DebugNetworkInfo();
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    APawn* Player = Cast<APawn>(OtherActor);
    if (!Player || !Player->IsPlayerControlled()) return;

    ACSPlayerState* PS = Player->GetPlayerState<ACSPlayerState>();
    if (!PS) return;

    PS->SetPersonalRespawnPoint(ConnectedRespawnPoint);

    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Personal respawn point set for %s"),
        *Player->GetName());
}

void ACSCheckPoint::DebugNetworkInfo() const
{
    UWorld* World = GetWorld();
    if (!World) return;

    const ENetMode NetMode = World->GetNetMode();
    FString NetModeString;

    switch (NetMode)
    {
    case NM_Standalone:      NetModeString = TEXT("Standalone");      break;
    case NM_DedicatedServer: NetModeString = TEXT("DedicatedServer"); break;
    case NM_ListenServer:    NetModeString = TEXT("ListenServer");    break;
    case NM_Client:          NetModeString = TEXT("Client");          break;
    default:                 NetModeString = TEXT("Unknown");         break;
    }

    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint Network Mode: %s, HasAuthority: %s"),
        *NetModeString,
        HasAuthority() ? TEXT("True") : TEXT("False"));
}
