// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSGravityCoreSphere.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Character/CSCharacterBase.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"


ACSGravityCoreSphere::ACSGravityCoreSphere()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;

	// GravitySphereTrigger
	SphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GravitySphereTrigger"));
	SphereTrigger->SetSphereRadius(GravityInfluenceRange, true);
	SphereTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	SphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	SphereTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RootComponent = SphereTrigger;
	SphereTrigger->SetIsReplicated(true);

	SphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerBeginOverlap);
	SphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnTriggerEndOverlap);
	SphereTrigger->SetCollisionResponseToChannel(
		CCHANNEL_CSGRAVITY_CORE_AFFECTED,
		ECR_Overlap
	);
	SphereTrigger->SetCollisionResponseToChannel(
		CCHANNEL_CSGRAVITY_CORE,
		ECR_Ignore
	);

	// Static Mesh
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	StaticMeshComp->SetupAttachment(SphereTrigger); 
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshComp->SetIsReplicated(true);
	StaticMeshComp->OnComponentBeginOverlap.AddDynamic(this, &ACSGravityCoreSphere::OnCoreBeginOverlap);

	MeshRadius = 50.0f;
}

void ACSGravityCoreSphere::BeginPlay()
{
	Super::BeginPlay();

	if ( !StaticMesh.IsValid() )
	{
		StaticMesh.LoadSynchronous();
	}
	
	if ( StaticMesh.IsValid() && IsValid( StaticMeshComp ) )
	{
		StaticMeshComp->SetStaticMesh(StaticMesh.Get());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("StaticMesh failed to load in ACSGravityCoreSphere"));
	}

	// AttachToActor 로 붙은 액터는 부모가 파괴돼도 "분리"될 뿐 파괴되지 않는다.
	// 그래서 코어를 켠 채로 죽으면(리스폰이 구 Pawn 을 Destroy) 코어가 죽은 자리에 그대로 남았다.
	// 소유 액터의 파괴를 구독해 같이 사라지게 한다. (bReplicates 라 서버에서 파괴하면 클라에서도 사라진다)
	if (HasAuthority())
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->OnDestroyed.AddDynamic(this, &ACSGravityCoreSphere::HandleOwnerDestroyed);
		}
	}

	UE_LOG(LogCS, Log, TEXT("ACSGravityCoreSphere BeginPlay: %s"), *GetNameSafe(GetOwner()));
}

void ACSGravityCoreSphere::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnDestroyed.RemoveDynamic(this, &ACSGravityCoreSphere::HandleOwnerDestroyed);
	}

	Super::EndPlay(EndPlayReason);
}

void ACSGravityCoreSphere::HandleOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void ACSGravityCoreSphere::Tick(float DeltaSeconds)
{
	// Super 를 빼면 BP 자식의 Event Tick 이 서버/클라 어디서도 돌지 않는다
	Super::Tick(DeltaSeconds);

	if (!HasAuthority()) return;

	// 사망 -> 리스폰 사이에 구 Pawn 은 ReviveDelay(기본 1초) 동안 살아 있다.
	// 그동안 숨겨진 시체에 붙은 코어가 죽은 자리에 떠 있으므로 사망 시점에 바로 정리한다.
	if (const ACSCharacterBase* OwnerCharacter = Cast<ACSCharacterBase>(GetOwner()))
	{
		if (OwnerCharacter->IsDead())
		{
			Destroy();
			return;
		}
	}

	//ProcessForCharacter(DeltaSeconds);
	ProcessForStaticMesh(DeltaSeconds);
}

void ACSGravityCoreSphere::ProcessForStaticMesh(float DeltaTime)
{
	if( !IsValid(SphereTrigger) ) return;
	const FVector CoreLocation = SphereTrigger->GetComponentLocation();
	const FVector CoreVelocity = IsValid(GetOwner()) ? GetOwner()->GetVelocity() : FVector::ZeroVector;

	for (auto It = StaticMeshesInSphereTrigger.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Mesh = It->Get();

		if (!IsValid(Mesh))
		{
			It.RemoveCurrent();
			continue;
		}

		if (!Mesh->IsSimulatingPhysics())
		{
			continue;
		}

		if ( IsValid( Mesh->GetOwner() ) )
		{
			if ( UCSMeshAffectedByGravityCore* MeshAffectedByGravityCore = Mesh->GetOwner()->FindComponentByClass<UCSMeshAffectedByGravityCore>(); IsValid( MeshAffectedByGravityCore ) )
			{
				if ( !MeshAffectedByGravityCore->IsEnable() )
					continue;
			}
		}

		const FVector MeshLocation = Mesh->GetComponentLocation();
		const FVector ToBH = CoreLocation - MeshLocation;
		const float Distance = ToBH.Size();
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Direction = ToBH.GetSafeNormal();
		const float DistanceAlpha = FMath::Clamp((Distance - MeshRadius) / GravityInfluenceRange, 0.01f, 1.0f);
		const FVector MeshVelocity = Mesh->GetPhysicsLinearVelocity();
		const FVector RelativeVelocity = MeshVelocity - CoreVelocity;
		const FVector PullAcceleration = Direction * PullStrength * DistanceAlpha;
		const FVector DampingAcceleration = -RelativeVelocity * PullDamping;

		Mesh->AddForce(PullAcceleration + DampingAcceleration, NAME_None, true);

		const FVector CurrentRelativeVelocity = Mesh->GetPhysicsLinearVelocity() - CoreVelocity;
		if (CurrentRelativeVelocity.SizeSquared() > FMath::Square(MaxPullSpeed))
		{
			Mesh->SetPhysicsLinearVelocity(CoreVelocity + CurrentRelativeVelocity.GetClampedToMaxSize(MaxPullSpeed), false);
		}
	}
}

void ACSGravityCoreSphere::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	if (!HasAuthority()) return;

	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		if ( bCheckMeshHaveComponent && OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>() == nullptr ) return;

		// 원래 값을 저장해 EndOverlap 에서 되돌린다
		// (예전엔 복원하지 않아, 한 번이라도 코어에 닿은 프롭은 이후 영원히 느리게 굴렀다)
		if (!SavedMeshStates.Contains(TargetStaticMeshComp))
		{
			FCSGravityCoreMeshState Saved;
			if (const FBodyInstance* Body = TargetStaticMeshComp->GetBodyInstance())
			{
				Saved.MaxAngularVelocityRad = Body->GetMaxAngularVelocityInRadians();
			}
			Saved.AngularDamping = TargetStaticMeshComp->GetAngularDamping();
			Saved.bGravityEnabled = TargetStaticMeshComp->IsGravityEnabled();
			SavedMeshStates.Add(TargetStaticMeshComp, Saved);
		}

		TargetStaticMeshComp->SetPhysicsMaxAngularVelocityInDegrees(180.f);
		TargetStaticMeshComp->SetAngularDamping(2.0f);

		TargetStaticMeshComp->SetEnableGravity(false);
		StaticMeshesInSphereTrigger.Add(TargetStaticMeshComp);

		if (UCSMeshAffectedByGravityCore* Comp = OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>())
		{
			Comp->AddInfluence();
		}
	}
}

void ACSGravityCoreSphere::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;

	// For StaticMesh
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		if (bCheckMeshHaveComponent && OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>() == nullptr) return;
		
		StaticMeshesInSphereTrigger.Remove(TargetStaticMeshComp);

		UCSMeshAffectedByGravityCore* Comp = OtherActor->FindComponentByClass<UCSMeshAffectedByGravityCore>();
		if (Comp)
		{
			Comp->RemoveInfluence();
		}

		// 다른 코어/블랙홀이 아직 이 메시를 잡고 있으면 물리 상태를 되돌리지 않는다
		// (예전엔 먼저 빠져나간 쪽이 중력을 켜고 속도를 0 으로 만들어, 끌려가던 프롭이 갑자기 멈췄다)
		if (Comp && Comp->IsInfluenced())
		{
			return;
		}

		if (const FCSGravityCoreMeshState* Saved = SavedMeshStates.Find(TargetStaticMeshComp))
		{
			TargetStaticMeshComp->SetPhysicsMaxAngularVelocityInRadians(Saved->MaxAngularVelocityRad);
			TargetStaticMeshComp->SetAngularDamping(Saved->AngularDamping);
			TargetStaticMeshComp->SetEnableGravity(Saved->bGravityEnabled);
			SavedMeshStates.Remove(TargetStaticMeshComp);
		}
		else
		{
			TargetStaticMeshComp->SetEnableGravity(true);
		}

		TargetStaticMeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
		TargetStaticMeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);

		UE_LOG(LogCS, Log, TEXT("ACSGravityCoreSphere - OnTriggerEndOverlap"));
	}
}

void ACSGravityCoreSphere::OnCoreBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	UStaticMeshComponent* TargetStaticMeshComp = Cast<UStaticMeshComponent>(OtherComp);
	if (TargetStaticMeshComp)
	{
		TargetStaticMeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
		TargetStaticMeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
	}
}
