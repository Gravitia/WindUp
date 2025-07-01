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

    // ��Ʈ��ũ ���� ����
    bReplicates = true;

    // �ʱⰪ
    ConnectedRespawnPoint = nullptr;
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);

    // ����� ���� ���
    DebugNetworkInfo();
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint OnTriggerBeginOverlap"));

    // ���������� ����
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Client detected, skipping GameMode access"));
        return;
    }

    APawn* Player = Cast<APawn>(OtherActor);
    if (Player && Player->IsPlayerControlled())
    {
        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Player detected: %s"), *Player->GetName());

        // GameMode �������� (����������)
        ACSGameMode* GameMode = GetCSGameMode();
        if (!GameMode)
        {
            UE_LOG(LogTemp, Warning, TEXT("CSLog : CheckPoint - GameMode is null!"));
            return;
        }

        if (!ConnectedRespawnPoint)
        {
            UE_LOG(LogTemp, Warning, TEXT("CSLog : CheckPoint - ConnectedRespawnPoint is null! Please set it in Blueprint."));
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Setting respawn point"));
        GameMode->SetCurrentRespawnPoint(ConnectedRespawnPoint);
    }
}

void ACSCheckPoint::DebugNetworkInfo() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        ENetMode NetMode = World->GetNetMode();
        FString NetModeString;

        switch (NetMode)
        {
        case NM_Standalone:
            NetModeString = "Standalone";
            break;
        case NM_DedicatedServer:
            NetModeString = "DedicatedServer";
            break;
        case NM_ListenServer:
            NetModeString = "ListenServer";
            break;
        case NM_Client:
            NetModeString = "Client";
            break;
        default:
            NetModeString = "Unknown";
            break;
        }

        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint Network Mode: %s, HasAuthority: %s"),
            *NetModeString,
            HasAuthority() ? TEXT("True") : TEXT("False"));
    }
}

void ACSCheckPoint::ServerActivateCheckpoint_Implementation(APawn* Player)
{
    // ���������� ����Ǵ� RPC
    UE_LOG(LogTemp, Log, TEXT("CSLog : ServerActivateCheckpoint called for player: %s"),
        Player ? *Player->GetName() : TEXT("NULL"));

    ACSGameMode* GameMode = GetCSGameMode();
    if (GameMode && ConnectedRespawnPoint)
    {
        GameMode->SetCurrentRespawnPoint(ConnectedRespawnPoint);
        UE_LOG(LogTemp, Log, TEXT("CSLog : Checkpoint activated via RPC"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CSLog : ServerActivateCheckpoint - GameMode or RespawnPoint is null"));
    }
}

ACSGameMode* ACSCheckPoint::GetCSGameMode() const
{
    // ���������� GameMode�� ���� ����
    if (HasAuthority())
    {
        return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
    }

    UE_LOG(LogTemp, Warning, TEXT("CSLog : GetCSGameMode called on client - returning nullptr"));
    return nullptr;
}