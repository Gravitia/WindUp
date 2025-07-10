// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/System/CSLevelStreamingTrigger.h"
#include "Subsystem/CSLevelStreamingSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ACSLevelStreamingTrigger::ACSLevelStreamingTrigger()
{
    PrimaryActorTick.bCanEverTick = false;
    ChapterNumber = 1;
    StageNumber = 1;

    GetCollisionComponent()->OnComponentBeginOverlap.AddDynamic(this, &ACSLevelStreamingTrigger::OnOverlapBegin);

    bReplicates = true;
}

void ACSLevelStreamingTrigger::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("CSLevelStreamingTrigger initialized for C%d_S%d"), ChapterNumber, StageNumber);
}

void ACSLevelStreamingTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 로컬 플레이어만 트리거 가능 (서버/클라이언트 구분 없이)
    if (!IsValidPlayer(OtherActor))
    {
        return;
    }

    UCSLevelStreamingSubsystem* LevelSubsystem = GetLevelStreamingSubsystem();
    if (!LevelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get CSLevelStreamingSubsystem"));
        return;
    }

    // 새로운 RequestStreamLevel 함수 사용 (자동으로 서버/클라이언트 처리)
    if (ChapterNumber > 0 && StageNumber > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Requesting level streaming for stage: C%d_S%d"), ChapterNumber, StageNumber);
        LevelSubsystem->RequestStreamLevel(ChapterNumber, StageNumber);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid chapter/stage numbers: C%d_S%d"), ChapterNumber, StageNumber);
    }
}

bool ACSLevelStreamingTrigger::IsValidPlayer(AActor* Actor)
{
    ACharacter* Character = Cast<ACharacter>(Actor);
    if (!Character)
    {
        return false;
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC)
    {
        return false;
    }

    // 로컬 플레이어만 트리거 가능 (서버/클라이언트 모두 허용)
    return PC->IsLocalPlayerController();
}

UCSLevelStreamingSubsystem* ACSLevelStreamingTrigger::GetLevelStreamingSubsystem() const
{
    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UCSLevelStreamingSubsystem>();
    }
    return nullptr;
}