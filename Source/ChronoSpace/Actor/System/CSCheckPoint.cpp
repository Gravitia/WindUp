// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSKillZone.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Game/CSGameMode.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Kismet/GameplayStatics.h"

ACSCheckPoint::ACSCheckPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // TriggerBox as root
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionProfileName("Trigger");

    // 네트워크 복제 설정
    bReplicates = true;

    // 초기값
    ConnectedRespawnPoint = nullptr;
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);

    // 디버그 정보 출력
    DebugNetworkInfo();
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint OnTriggerBeginOverlap"));

    // 서버에서만 실행
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Client detected, skipping GameMode access"));
        return;
    }

    APawn* Player = Cast<APawn>(OtherActor);
    if (Player && Player->IsPlayerControlled())
    {
        UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Player detected: %s"), *Player->GetName());

        // GameMode 가져오기 (서버에서만)
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

        if (SaveStageClear)
        {
            UCSGameProgressSubsystem* ProgressSubsystem = GetGameProgressSubsystem();
            if (ProgressSubsystem)
            {
                ProgressSubsystem->ClearStage(CurrentChapterNumber, CurrentStageNumber);
                ProgressSubsystem->SetLastPlayedStage(CurrentChapterNumber, CurrentStageNumber);

                UE_LOG(LogTemp, Log, TEXT("CSLog : Stage cleared and saved: C%d_S%d"),
                    CurrentChapterNumber, CurrentStageNumber);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CSLog : GameProgressSubsystem not found for stage save"));
            }
        }
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
    // 서버에서만 실행되는 RPC
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
    // 서버에서만 GameMode에 접근 가능
    if (HasAuthority())
    {
        return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
    }

    UE_LOG(LogTemp, Warning, TEXT("CSLog : GetCSGameMode called on client - returning nullptr"));
    return nullptr;
}

UCSGameProgressSubsystem* ACSCheckPoint::GetGameProgressSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        return GameInstance->GetSubsystem<UCSGameProgressSubsystem>();
    }
    return nullptr;
}