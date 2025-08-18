// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSBlackHole.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "Physics/CSCollision.h"
#include "Kismet/GameplayStatics.h"
#include "ActorComponent/CSCharacterPulledByBlackhole.h"
#include "ChronoSpace.h"

// Sets default values
ACSBlackHole::ACSBlackHole()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;

	// GravitySphereTrigger
	GravitySphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GravitySphereTrigger"));
	RootComponent = GravitySphereTrigger;
	GravitySphereTrigger->SetSphereRadius(GravityInfluenceRange, true);
	GravitySphereTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	GravitySphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	GravitySphereTrigger->SetIsReplicated(true);

	GravitySphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSBlackHole::OnTriggerBeginOverlap);
	GravitySphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSBlackHole::OnTriggerEndOverlap);

	// EventHorizonSphereTrigger
	EventHorizonSphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("EventHorizonSphereTrigger"));
	EventHorizonSphereTrigger->SetupAttachment(GravitySphereTrigger);
	EventHorizonSphereTrigger->SetSphereRadius(StopRadius, true);
	EventHorizonSphereTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	EventHorizonSphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	EventHorizonSphereTrigger->SetIsReplicated(true);

	EventHorizonSphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSBlackHole::OnStopTriggerBeginOverlap);
	EventHorizonSphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSBlackHole::OnStopTriggerEndOverlap);

	// Static Mesh
	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
	CoreMesh->SetupAttachment(GravitySphereTrigger); 
	CoreMesh->SetIsReplicated(true); 

	FieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FieldMesh"));
	FieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldMesh->SetupAttachment(CoreMesh); 
	FieldMesh->SetIsReplicated(true);
}

// Called when the game starts or when spawned
void ACSBlackHole::BeginPlay()
{
	Super::BeginPlay();
	
	GravitySphereTrigger->SetSphereRadius(GravityInfluenceRange, true);
}

// Called every frame
void ACSBlackHole::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if ( !HasAuthority() )
	{
		return;
	}

	FVector BlackHoleLocation = GravitySphereTrigger->GetComponentLocation();

	for (auto Char = CharactersInSphereTrigger.CreateIterator(); Char; ++Char)
	{
		if (Char.Value() == Owner) continue;

		if ( CharactersInEventHorizon.Contains(Char.Value()) )
		{
			Char.Value()->GetCharacterMovement()->StopMovementImmediately();
			continue;
		}

		if (IsValid(Char.Value()))
		{
			FVector Power(1000.0f, 1000.0f, 1000.0f);
			FVector TargetLocation = Char.Value()->GetActorLocation();
			FVector Distance = BlackHoleLocation - TargetLocation;
			FVector Direction = Distance.GetSafeNormal();

			if (Distance.Size() < StopRadius)
			{
				Char.Value()->GetCharacterMovement()->StopMovementImmediately();
				continue;
			}

			Char.Value()->GetCharacterMovement()->AddImpulse(Direction * Power * PullStrength * DeltaTime, /*bVelocityChange=*/true);
			//Char.Value()->GetCharacterMovement()->AddForce(Direction * Power * PullStrength);
		}
	}
}

void ACSBlackHole::SetDuration(float Duration)
{
	SetLifeSpan(Duration);
}

void ACSBlackHole::SetGravityInfluenceRange(float Range)
{
	GravityInfluenceRange = Range;
	GravitySphereTrigger->SetSphereRadius(Range, true);

	if (GravitySphereTrigger)
	{
		FVector SphereLocation = GravitySphereTrigger->GetComponentLocation();
		float SphereRadius = GravitySphereTrigger->GetScaledSphereRadius();

		DrawDebugSphere(
			GetWorld(),
			SphereLocation,
			SphereRadius,
			12,				// 세그먼트 수 (구의 매끄러움)
			FColor::Green,
			false,			// 지속 표시
			5,   // 지속 시간 
			0,				// 디버그 선 우선순위
			2.0f			// 선 두께
		);
	}

	FieldMesh->SetRelativeScale3D( FVector( GravityInfluenceRange / MeshRadius) ); 
}

void ACSBlackHole::SetStopRange(float Range)
{
	StopRadius = Range;
}

void ACSBlackHole::SetPullStrength(float Strength)
{
	PullStrength = Strength;
}

void ACSBlackHole::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInSphereTrigger.Emplace(DetectedCharacter->GetFName(), DetectedCharacter);
	}
}

void ACSBlackHole::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInSphereTrigger.Remove(DetectedCharacter->GetFName());
	}
}

void ACSBlackHole::OnStopTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInEventHorizon.Add(DetectedCharacter);
	}
}

void ACSBlackHole::OnStopTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInEventHorizon.Remove(DetectedCharacter);
	}
}

