// Fill out your copyright notice in the Description page of Project Settings.

#include "CSBellows.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"

ACSBellows::ACSBellows()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellowsMesh"));
    RootComponent = Mesh;

    PressTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PressTrigger"));
    PressTrigger->SetupAttachment(RootComponent);
    PressTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PressTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    PressTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PressTrigger->SetGenerateOverlapEvents(true);

    // 초기 Mesh 스케일
    Mesh->SetRelativeScale3D(IdleScale);
}

void ACSBellows::BeginPlay()
{
    Super::BeginPlay();

    if (PressTrigger)
    {
        PressTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSBellows::OnTriggerBeginOverlap);
        PressTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSBellows::OnTriggerEndOverlap);
    }

    if (Mesh)
    {
        Mesh->SetRelativeScale3D(IdleScale);
    }
}

void ACSBellows::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bScaleLerping && Mesh)
    {
        ScaleLerpAlpha += DeltaSeconds / ScaleLerpDuration;

        const float Alpha = FMath::Clamp(ScaleLerpAlpha, 0.f, 1.f);
        const FVector NewScale = FMath::Lerp(StartScale, TargetScale, Alpha);
        Mesh->SetRelativeScale3D(NewScale);

        if (Alpha >= 1.f)
        {
            bScaleLerping = false;
        }
    }

    TickLinkedActorLerp(DeltaSeconds);
}

void ACSBellows::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (!HasAuthority())
    {
        return;
    }

    ACharacter* Ch = Cast<ACharacter>(OtherActor);
    if (!Ch)
    {
        return;
    }

    OverlappingPlayers.Add(Ch);

    if (bHoldPressTwoState)
    {
        return;
    }

    RecomputeStateFromOverlap();
}

void ACSBellows::OnTriggerEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex
)
{
    if (!HasAuthority())
    {
        return;
    }

    ACharacter* Ch = Cast<ACharacter>(OtherActor);
    if (!Ch)
    {
        return;
    }

    OverlappingPlayers.Remove(Ch);

    if (bHoldPressTwoState)
    {
        return;
    }

    RecomputeStateFromOverlap();
}

void ACSBellows::RecomputeStateFromOverlap()
{
    for (auto It = OverlappingPlayers.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }

    if (bHoldPressTwoState)
    {
        return;
    }

    const int32 Count = OverlappingPlayers.Num();

    EBellowsState NewState = EBellowsState::Idle;

    if (Count == 1)
    {
        NewState = EBellowsState::PressOnePlayer;
    }
    else if (Count >= 2)
    {
        NewState = EBellowsState::PressTwoPlayer;
    }

    if (BellowsState != NewState)
    {
        ApplyState(NewState);
    }
}

void ACSBellows::ApplyState(EBellowsState NewState)
{
    BellowsState = NewState;

    switch (NewState)
    {
    case EBellowsState::Idle:
        StartScaleLerp(IdleScale);
        break;

    case EBellowsState::PressOnePlayer:
        StartScaleLerp(PressOneScale);
        break;

    case EBellowsState::PressTwoPlayer:
        StartScaleLerp(PressTwoScale);
        StartPressTwoHold();

        if (!bLinkedMoved)
        {
            StartLinkedActorLerp();
        }
        break;

    default:
        break;
    }
}

void ACSBellows::StartScaleLerp(const FVector& NewTargetScale)
{
    if (!Mesh)
    {
        return;
    }

    StartScale = Mesh->GetRelativeScale3D();
    TargetScale = NewTargetScale;
    ScaleLerpAlpha = 0.f;
    bScaleLerping = true;
}

void ACSBellows::StartPressTwoHold()
{
    bHoldPressTwoState = true;

    GetWorldTimerManager().ClearTimer(TimerHandle_PressTwoHold);
    GetWorldTimerManager().SetTimer(
        TimerHandle_PressTwoHold,
        this,
        &ACSBellows::EndPressTwoHold,
        PressTwoHoldTime,
        false
    );
}

void ACSBellows::EndPressTwoHold()
{
    bHoldPressTwoState = false;
    RecomputeStateFromOverlap();
}

void ACSBellows::StartLinkedActorLerp()
{
    if (!LinkedActor)
    {
        return;
    }

    LinkedStartLoc = LinkedActor->GetActorLocation();

    const FVector BellowsBackwardDir = -GetActorForwardVector();
    LinkedTargetLoc = LinkedStartLoc + (BellowsBackwardDir * LinkedMoveDistance);

    LinkedLerpAlpha = 0.f;
    bLinkedLerping = true;
    bLinkedMoved = true;
}

void ACSBellows::OnRep_LinkedMoved()
{
    // 이미 옮겨진 상태로 접속했는데 러프가 돌지 않는 경우(늦은 접속) 최종 위치로 맞춘다.
    if (bLinkedMoved && !bLinkedLerping && IsValid(LinkedActor))
    {
        const FVector BellowsBackwardDir = -GetActorForwardVector();
        LinkedActor->SetActorLocation(LinkedActor->GetActorLocation() + BellowsBackwardDir * LinkedMoveDistance);
    }
}

void ACSBellows::TickLinkedActorLerp(float DeltaTime)
{
    if (!bLinkedLerping || !LinkedActor)
    {
        return;
    }

    LinkedLerpAlpha += DeltaTime / LinkedMoveDuration;
    const float Alpha = FMath::Clamp(LinkedLerpAlpha, 0.f, 1.f);

    const FVector NewLoc = FMath::Lerp(LinkedStartLoc, LinkedTargetLoc, Alpha);
    LinkedActor->SetActorLocation(NewLoc);

    if (Alpha >= 1.f)
    {
        bLinkedLerping = false;
    }
}

void ACSBellows::OnRep_BellowsState()
{
    switch (BellowsState)
    {
    case EBellowsState::Idle:
        StartScaleLerp(IdleScale);
        break;

    case EBellowsState::PressOnePlayer:
        StartScaleLerp(PressOneScale);
        break;

    case EBellowsState::PressTwoPlayer:
        StartScaleLerp(PressTwoScale);

        if (!bLinkedMoved)
        {
            StartLinkedActorLerp();
        }
        break;

    default:
        break;
    }
}

void ACSBellows::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSBellows, BellowsState);
    DOREPLIFETIME(ACSBellows, bLinkedMoved);
}