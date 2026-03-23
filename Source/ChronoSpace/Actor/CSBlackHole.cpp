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
#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Components/StaticMeshComponent.h"
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

	if (BlackHoleOnSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BlackHoleOnSound,
			GetActorLocation()
		);
	}
}

// Called every frame
void ACSBlackHole::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if ( !HasAuthority() )
	{
		return;
	}

	ProcessForCharacter(DeltaTime);
	ProcessForStaticMesh(DeltaTime);
}

void ACSBlackHole::SetDuration(float Duration)
{
	if ( Duration > 0 )
	{
		SetLifeSpan(Duration);
	}
}

void ACSBlackHole::Destroyed()
{
	// 블랙홀 파괴 시, 중력 범위 안에 남아있는 메쉬들의 상태를 안전하게 복원
	for (auto It = StaticMeshesInSphereTrigger.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->Get();
		if (IsValid(Mesh))
		{
			Mesh->SetEnableGravity(true);
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
			Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
			Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);

			AActor* MeshOwner = Mesh->GetOwner();
			if (MeshOwner)
			{
				if (UCSMeshPulledByBlackhole* Comp = MeshOwner->FindComponentByClass<UCSMeshPulledByBlackhole>())
				{
					Comp->NotifyInteractionEnded();
				}
			}
		}
	}
	StaticMeshesInSphereTrigger.Empty();

	// EventHorizon 안에만 있고 바깥 트리거에서 EndOverlap이 오지 않은 메쉬들 처리
	for (auto It = StaticMeshesInEventHorizon.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->Get();
		if (IsValid(Mesh))
		{
			AActor* MeshOwner = Mesh->GetOwner();
			if (MeshOwner)
			{
				if (UCSMeshPulledByBlackhole* Comp = MeshOwner->FindComponentByClass<UCSMeshPulledByBlackhole>())
				{
					Comp->NotifyInteractionEnded();
				}
			}
		}
	}
	StaticMeshesInEventHorizon.Empty();

	// BlackHole OFF Sound
	if (BlackHoleOffSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BlackHoleOffSound,
			GetActorLocation()
		);
	}

	Super::Destroyed();
}

void ACSBlackHole::SetGravityInfluenceRange(float Range)
{
	GravityInfluenceRange = Range;
	GravitySphereTrigger->SetSphereRadius(Range, true);

	if (GravitySphereTrigger)
	{
		FVector SphereLocation = GravitySphereTrigger->GetComponentLocation();
		float SphereRadius = GravitySphereTrigger->GetScaledSphereRadius();
		/*
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
		*/
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

void ACSBlackHole::SetCheckComponentInMesh(bool bCheckComponent)
{
	bCheckMeshHaveComponent = bCheckComponent;
}

void ACSBlackHole::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	// For Character
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if ( DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>() )
	{
		CharactersInSphereTrigger.Add(DetectedCharacter);
		return;
	}

	// For StaticMesh
	UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if ( StaticMeshComp )
	{
		StaticMeshComp->SetEnableGravity(false);
		StaticMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		StaticMeshesInSphereTrigger.Add(StaticMeshComp);

		if (OtherActor)
		{
			if (UCSMeshPulledByBlackhole* Comp = OtherActor->FindComponentByClass<UCSMeshPulledByBlackhole>())
			{
				Comp->NotifyInteractionStarted();
			}
		}
	}
}

void ACSBlackHole::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// For Character
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInSphereTrigger.Remove(DetectedCharacter);
	}

	// For StaticMesh
	UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (StaticMeshComp)
	{
		StaticMeshComp->SetEnableGravity(true);
		StaticMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		StaticMeshesInSphereTrigger.Remove(StaticMeshComp);

		if (OtherActor)
		{
			if (UCSMeshPulledByBlackhole* Comp = OtherActor->FindComponentByClass<UCSMeshPulledByBlackhole>())
			{
				// EventHorizon 안에 있으면 아직 상호작용 중 → 종료 알림 생략
				if (!StaticMeshesInEventHorizon.Contains(StaticMeshComp))
				{
					Comp->NotifyInteractionEnded();
				}
			}
		}
	}
}

void ACSBlackHole::OnStopTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	// For Character
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInEventHorizon.Add(DetectedCharacter);
	}

	// For StaticMesh
	UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (StaticMeshComp)
	{
		StaticMeshesInEventHorizon.Add(StaticMeshComp);
	}
}

void ACSBlackHole::OnStopTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// For Character
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);

	if (DetectedCharacter && DetectedCharacter->GetComponentByClass<UCSCharacterPulledByBlackhole>())
	{
		CharactersInEventHorizon.Remove(DetectedCharacter);
	}

	// For StaticMesh
	UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (StaticMeshComp)
	{
		StaticMeshesInEventHorizon.Remove(StaticMeshComp);
	}
}

void ACSBlackHole::ProcessForCharacter(float DeltaTime)
{
	FVector BlackHoleLocation = GravitySphereTrigger->GetComponentLocation();

	for (auto Char = CharactersInSphereTrigger.CreateIterator(); Char; ++Char)
	{
		if (Char->Get() == nullptr) continue;
		if (Char->Get() == Owner) continue;

		if (CharactersInEventHorizon.Contains(Char->Get()))
		{
			Char->Get()->GetCharacterMovement()->StopMovementImmediately();
			continue;
		}

		FVector Power(1000.0f, 1000.0f, 1000.0f);
		FVector TargetLocation = Char->Get()->GetActorLocation();
		FVector Distance = BlackHoleLocation - TargetLocation;
		FVector Direction = Distance.GetSafeNormal();

		if (Distance.Size() < StopRadius)
		{
			Char->Get()->GetCharacterMovement()->StopMovementImmediately();
			continue;
		}

		Char->Get()->GetCharacterMovement()->AddImpulse(Direction * Power * PullStrength * DeltaTime, /*bVelocityChange=*/true);	
	}
}

void ACSBlackHole::ProcessForStaticMesh(float DeltaTime)
{
	const FVector BlackHoleLocation = GravitySphereTrigger->GetComponentLocation();

	for (auto It = StaticMeshesInSphereTrigger.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->Get();

		if (!IsValid(Mesh))
		{
			It.RemoveCurrent();
			continue;
		}
		
		if ( bCheckMeshHaveComponent && 
			 Mesh->GetOwner() &&
			 Mesh->GetOwner()->FindComponentByClass<UCSMeshPulledByBlackhole>() == nullptr ) 
			continue;

		if (!Mesh->IsSimulatingPhysics())
			continue;

		if (StaticMeshesInEventHorizon.Contains(Mesh))
		{
			Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
			Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
			continue;
		}

		FVector Power(1000.0f, 1000.0f, 1000.0f);
		const FVector MeshLocation = Mesh->GetComponentLocation();
		const FVector ToBH = BlackHoleLocation - MeshLocation;
		const float Distance = ToBH.Size();

		float MeshMass = FMath::Max( Mesh->GetMass(), 100.0f);

		Power *= (MeshMass / 100);

		if (Distance < StopRadius)
		{
			Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
			Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
			continue;
		}

		const FVector Direction = ToBH.GetSafeNormal();
		Mesh->AddImpulse(Direction * Power * PullStrength * DeltaTime, NAME_None, true);
	}
}

