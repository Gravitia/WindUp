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
	// 블랙홀 파괴 시, 영향 아래 있던 메시 상태를 복원한다.
	// 두 집합에 모두 들어 있는 메시를 두 번 해제하지 않도록 합집합으로 한 번만 처리한다
	// (예전엔 같은 메시에 종료 알림이 두 번 가서 BP 토글 연출이 두 번 뒤집혔다).
	TSet< TWeakObjectPtr<UStaticMeshComponent> > AffectedMeshes = StaticMeshesInSphereTrigger;
	AffectedMeshes.Append(StaticMeshesInEventHorizon);

	StaticMeshesInSphereTrigger.Empty();
	StaticMeshesInEventHorizon.Empty();

	for (const TWeakObjectPtr<UStaticMeshComponent>& WeakMesh : AffectedMeshes)
	{
		UStaticMeshComponent* Mesh = WeakMesh.Get();
		if (!IsValid(Mesh)) continue;

		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);

		AActor* MeshOwner = Mesh->GetOwner();
		if (UCSMeshPulledByBlackhole* Comp = MeshOwner ? MeshOwner->FindComponentByClass<UCSMeshPulledByBlackhole>() : nullptr)
		{
			// 저장해 둔 원래 중력/카메라 응답으로 되돌린다 (다른 블랙홀이 아직 잡고 있으면 유지)
			Comp->RemoveInfluence();
		}
		else if (Mesh->IsSimulatingPhysics())
		{
			Mesh->SetEnableGravity(true);
		}
	}

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
	// 물리/콜리전 상태 변경은 서버 권한.
	// 클라 복제본의 GravitySphereTrigger 반경은 복제되지 않아(기본값 유지) 서버와 판정 범위가 달랐고,
	// 끌어당기는 Tick 자체도 서버 전용이라 클라의 로컬 변경은 어긋나기만 했다.
	if (!HasAuthority()) return;

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
		StaticMeshesInSphereTrigger.Add(StaticMeshComp);

		// 물리 시뮬레이션 중인 메시만 건드린다.
		// (예전엔 바닥/벽 같은 레벨 지오메트리까지 카메라 채널을 Ignore 로 바꿔 스프링암 카메라가 벽을 뚫었다)
		if (!StaticMeshComp->IsSimulatingPhysics()) return;

		if (UCSMeshPulledByBlackhole* Comp = OtherActor ? OtherActor->FindComponentByClass<UCSMeshPulledByBlackhole>() : nullptr)
		{
			// 컴포넌트가 원래 물리 상태를 저장/복원하고, 복제로 클라에도 연출(버튼 점등)을 전달한다
			Comp->AddInfluence();
		}
		else
		{
			StaticMeshComp->SetEnableGravity(false);
		}
	}
}

void ACSBlackHole::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;

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
		StaticMeshesInSphereTrigger.Remove(StaticMeshComp);

		if (UCSMeshPulledByBlackhole* Comp = OtherActor ? OtherActor->FindComponentByClass<UCSMeshPulledByBlackhole>() : nullptr)
		{
			// EventHorizon 안에 있으면 아직 상호작용 중 -> 해제 생략
			if (!StaticMeshesInEventHorizon.Contains(StaticMeshComp))
			{
				// 다른 블랙홀이 아직 잡고 있으면 컴포넌트가 카운트로 걸러 원복하지 않는다
				Comp->RemoveInfluence();
			}
		}
		else if (StaticMeshComp->IsSimulatingPhysics())
		{
			StaticMeshComp->SetEnableGravity(true);
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

