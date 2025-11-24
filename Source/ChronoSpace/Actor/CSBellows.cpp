// Fill out your copyright notice in the Description page of Project Settings.


#include "CSBellows.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"

ACSBellows::ACSBellows()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(true);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BellowsMesh"));
    RootComponent = Mesh;
}

void ACSBellows::BeginPlay()
{
    Super::BeginPlay();
}

void ACSBellows::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Bellows scale lerp
    if (bScaleLerping)
    {
        ScaleLerpAlpha += DeltaSeconds / ScaleLerpDuration;

        FVector NewScale = FMath::Lerp(StartScale, TargetScale, ScaleLerpAlpha);
        SetActorScale3D(NewScale);

        if (ScaleLerpAlpha >= 1.f)
            bScaleLerping = false;
    }

    // Linked actor lerp movement
    TickLinkedActorLerp(DeltaSeconds);
}


// --------------------------
// Character Landed Event
// --------------------------
void ACSBellows::NotifyPlayerLanded(ACharacter* PlayerCharacter)
{
    if (!HasAuthority()) return;

    switch (BellowsState)
    {
    case EBellowsState::Idle:
        ChangeState(EBellowsState::PressOnePlayer);
        break;

    case EBellowsState::PressOnePlayer:
        ChangeState(EBellowsState::PressTwoPlayer);
        break;
    }
}


// --------------------------
// State Machine
// --------------------------
void ACSBellows::ChangeState(EBellowsState NewState)
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

        // Only move once
        if (!bLinkedMoved)
        {
            StartLinkedActorLerp();
        }
        break;
    }

    // reset only Bellows, not linked actor
    GetWorldTimerManager().ClearTimer(TimerHandle_Reset);
    GetWorldTimerManager().SetTimer(
        TimerHandle_Reset,
        this,
        &ACSBellows::ResetToIdle,
        0.5f,
        false
    );
}

void ACSBellows::ResetToIdle()
{
    BellowsState = EBellowsState::Idle;
    StartScaleLerp(IdleScale);
}


// --------------------------
// Scale Lerp
// --------------------------
void ACSBellows::StartScaleLerp(const FVector& NewTargetScale)
{
    StartScale = GetActorScale3D();
    TargetScale = NewTargetScale;
    ScaleLerpAlpha = 0.f;
    bScaleLerping = true;
}


// --------------------------
// Linked Actor LERP Movement
// --------------------------

void ACSBellows::StartLinkedActorLerp()
{
    if (!LinkedActor) return;

    LinkedStartLoc = LinkedActor->GetActorLocation();
    LinkedTargetLoc = LinkedStartLoc + FVector(0.f, LinkedMoveDistance, 0.f);

    LinkedLerpAlpha = 0.f;
    bLinkedLerping = true;
    bLinkedMoved = true;
}

void ACSBellows::TickLinkedActorLerp(float DeltaTime)
{
    if (!bLinkedLerping || !LinkedActor) return;

    LinkedLerpAlpha += DeltaTime / LinkedMoveDuration;
    FVector NewLoc = FMath::Lerp(LinkedStartLoc, LinkedTargetLoc, LinkedLerpAlpha);

    LinkedActor->SetActorLocation(NewLoc);

    if (LinkedLerpAlpha >= 1.f)
        bLinkedLerping = false;
}


// --------------------------
// RepNotify
// --------------------------
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
    }
}


// --------------------------
// Replication
// --------------------------
void ACSBellows::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACSBellows, BellowsState);
}
