// Fill out your copyright notice in the Description page of Project Settings.


#include "CSBellows.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
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

    // -----------------------------
    // Scale Lerp Animation
    // -----------------------------
    if (bScaleLerping)
    {
        ScaleLerpAlpha += DeltaSeconds / ScaleLerpDuration;

        FVector NewScale = FMath::Lerp(StartScale, TargetScale, ScaleLerpAlpha);
        SetActorScale3D(NewScale);

        if (ScaleLerpAlpha >= 1.f)
        {
            bScaleLerping = false;
        }
    }
}

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

    default:
        break;
    }
}

// ----------------------
// State Machine
// ----------------------
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

        // Push linked actor only once
        if (!bLinkedActorPushed)
        {
            PushLinkedActor();
            bLinkedActorPushed = true;
        }
        break;
    }

    // Auto Reset to Idle (LinkedActor does NOT reset)
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

// ----------------------
// Scale Lerp
// ----------------------
void ACSBellows::StartScaleLerp(const FVector& NewTargetScale)
{
    StartScale = GetActorScale3D();
    TargetScale = NewTargetScale;

    ScaleLerpAlpha = 0.f;
    bScaleLerping = true;
}

// ----------------------
// Push Linked Actor (Force)
// ----------------------
void ACSBellows::PushLinkedActor()
{
    if (!LinkedActor) return;

    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(LinkedActor->GetRootComponent());
    if (!Prim) return;

    if (!Prim->IsSimulatingPhysics())
    {
        Prim->SetSimulatePhysics(true);
    }

    // Add force (continuous force)
    Prim->AddForce(LinkedForce, NAME_None, true);

    // If you prefer instant impulse style:
    // Prim->AddImpulse(LinkedForce);
}

// ----------------------
// Rep Notify
// ----------------------
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

        if (!bLinkedActorPushed)
        {
            PushLinkedActor();
            bLinkedActorPushed = true;
        }
        break;
    }
}

void ACSBellows::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSBellows, BellowsState);
}
