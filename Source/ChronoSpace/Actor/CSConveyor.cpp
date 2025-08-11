// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSConveyor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ACSConveyor::ACSConveyor()
{
    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;

    // Enable replication
    bReplicates = true;

    // Create and setup Box component
    Box = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
    SetRootComponent(Box);

    // Setup collision for overlap events
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    Box->SetGenerateOverlapEvents(true);

    // Set default box extent
    Box->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));

    // Create and setup Direction arrow component
    Direction = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    Direction->SetupAttachment(Box);
    Direction->SetArrowColor(FLinearColor::Green);
    Direction->SetArrowSize(2.0f);

    // Bind overlap events - 수정된 부분: 올바른 클래스 이름 사용
    Box->OnComponentBeginOverlap.AddDynamic(this, &ACSConveyor::OnOverlapBegin);
    Box->OnComponentEndOverlap.AddDynamic(this, &ACSConveyor::OnOverlapEnd);
}

// Called when the game starts or when spawned
void ACSConveyor::BeginPlay()
{
    Super::BeginPlay();

    // Ensure this actor replicates for multiplayer
    SetReplicates(true);

    // Clear any existing overlapping primitives
    OverlappingPrims.Empty();
}

void ACSConveyor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // Validate parameters
    if (!OtherComp || !OtherActor || OtherActor == this)
    {
        return;
    }

    // Add to overlapping primitives set
    OverlappingPrims.Add(OtherComp);

    // Debug log (remove in production)
    UE_LOG(LogTemp, Log, TEXT("Conveyor Overlap Begin: %s"), *OtherActor->GetName());
}

void ACSConveyor::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Remove from overlapping primitives set
    if (OtherComp)
    {
        OverlappingPrims.Remove(OtherComp);

        // Debug log (remove in production)
        if (OtherActor)
        {
            UE_LOG(LogTemp, Log, TEXT("Conveyor Overlap End: %s"), *OtherActor->GetName());
        }
    }
}

// Called every frame
void ACSConveyor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Only apply forces on the server to avoid conflicts in multiplayer
    if (!HasAuthority())
    {
        return;
    }

    // Early exit if no speed or no overlapping objects
    if (FMath::IsNearlyZero(Speed) || OverlappingPrims.Num() == 0)
    {
        return;
    }

    // Get conveyor direction
    const FVector BeltDirection = Direction->GetForwardVector().GetSafeNormal();

    // Process all overlapping primitives
    for (auto It = OverlappingPrims.CreateIterator(); It; ++It)
    {
        // Clean up invalid weak pointers
        if (!It->IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        UPrimitiveComponent* Prim = It->Get();
        if (!IsValid(Prim))
        {
            It.RemoveCurrent();
            continue;
        }

        // Apply force based on object type
        if (Prim->IsSimulatingPhysics())
        {
            ApplyForceToPhysicsObject(Prim, BeltDirection, DeltaTime);
        }
        else if (bAffectCharacters)
        {
            AActor* PrimOwner = Prim->GetOwner();
            if (ACharacter* Character = Cast<ACharacter>(PrimOwner))
            {
                ApplyForceToCharacter(Character, BeltDirection, DeltaTime);
            }
        }
    }
}

void ACSConveyor::ApplyForceToPhysicsObject(UPrimitiveComponent* Prim, const FVector& BeltDirection, float DeltaTime)
{
    if (!IsValid(Prim) || !Prim->IsSimulatingPhysics())
    {
        return;
    }

    // Apply acceleration-based force (mass-independent)
    // Using bAccelChange=true makes the force independent of mass
    const FVector Force = BeltDirection * ForceScale;
    Prim->AddForce(Force, NAME_None, true); // bAccelChange = true
}

void ACSConveyor::ApplyForceToCharacter(ACharacter* Character, const FVector& BeltDirection, float DeltaTime)
{
    if (!IsValid(Character))
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
    if (!IsValid(MovementComp))
    {
        return;
    }

    // Only affect characters that are on the ground
    if (!MovementComp->IsMovingOnGround())
    {
        return;
    }

    // Get current player input direction
    const FVector CurrentVelocity = MovementComp->Velocity;
    const FVector CurrentInputVector = MovementComp->GetLastInputVector();

    // Calculate conveyor influence based on player input
    float ConveyorInfluence = 1.0f;

    // If player is actively moving against the conveyor, reduce conveyor influence
    if (!CurrentInputVector.IsNearlyZero())
    {
        const float DotProduct = FVector::DotProduct(CurrentInputVector.GetSafeNormal(), BeltDirection);

        // If player is moving opposite to conveyor (dot product < 0)
        if (DotProduct < 0.0f)
        {
            // Reduce conveyor influence, allowing player to move against it
            ConveyorInfluence = FMath::Clamp(1.0f + DotProduct, 0.2f, 1.0f); // Min 20% influence
        }
        else if (DotProduct > 0.0f)
        {
            // Player moving with conveyor, slightly boost the effect
            ConveyorInfluence = FMath::Clamp(1.0f + (DotProduct * 0.3f), 1.0f, 1.3f);
        }
    }

    // Use AddInputVector for more natural feel - this blends with player input
    const float NormalizedSpeed = Speed / FMath::Max(MovementComp->MaxWalkSpeed, 1.0f);
    const FVector ConveyorInput = BeltDirection * NormalizedSpeed * ConveyorInfluence;

    // Apply as input vector (blends naturally with player input)
    MovementComp->AddInputVector(ConveyorInput);

    // Optional: Add a small additional force for physics-based feel
    // This provides the "pushing" sensation while still allowing player control
    const FVector AdditionalForce = BeltDirection * (ForceScale * 0.1f) * ConveyorInfluence;
    MovementComp->AddForce(AdditionalForce);
}