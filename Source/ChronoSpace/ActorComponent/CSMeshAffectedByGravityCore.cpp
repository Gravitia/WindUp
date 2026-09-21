// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "Subsystem/CSCameraOcclusionFadeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Physics/CSCollision.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

UCSMeshAffectedByGravityCore::UCSMeshAffectedByGravityCore()
{
	// SetIsReplicated(true);

	SetIsReplicatedByDefault(true);
}

void UCSMeshAffectedByGravityCore::BeginPlay()
{
	Super::BeginPlay(); 

	AActor* Owner = GetOwner(); 
	if (!Owner) return; 

	UStaticMeshComponent* MeshComp = 
		Owner->FindComponentByClass<UStaticMeshComponent>(); 

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("No StaticMeshComponent found on %s"), *Owner->GetName()); 
		return;
	}

	Owner->SetReplicates(true);
	Owner->SetReplicateMovement(true);
	MeshComp->SetIsReplicated(true);

	if (!Owner->HasAuthority())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetEnableGravity(false);
	}

	MeshComp->SetCollisionObjectType(CCHANNEL_CSGRAVITY_CORE_AFFECTED);

	// 카메라 채널 무시는 액터의 모든 스태틱 메시에 건다. 첫 메시(몸통)에만 걸면 붙어 있는 버튼 같은
	// 나머지 메시가 스프링암을 밀어내고, 그러면서 페이드는 액터 전체에 걸려 두 증상이 동시에 난다.
	TInlineComponentArray<UStaticMeshComponent*> AllMeshes;
	Owner->GetComponents<UStaticMeshComponent>(AllMeshes);
	for (UStaticMeshComponent* M : AllMeshes)
	{
		if (IsValid(M)) M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// 카메라 채널을 무시하는 대신 캐릭터를 가릴 수 있으므로 페이드 대상으로 등록한다
	if (UCSCameraOcclusionFadeSubsystem* Fade = GetWorld()->GetSubsystem<UCSCameraOcclusionFadeSubsystem>())
	{
		Fade->RegisterTarget(Owner);
	}
}

void UCSMeshAffectedByGravityCore::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(GetWorld()) && IsValid(GetOwner()))
	{
		if (UCSCameraOcclusionFadeSubsystem* Fade = GetWorld()->GetSubsystem<UCSCameraOcclusionFadeSubsystem>())
		{
			Fade->UnregisterTarget(GetOwner());
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UCSMeshAffectedByGravityCore::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCSMeshAffectedByGravityCore, InfluenceCount);
}

void UCSMeshAffectedByGravityCore::AddInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	++InfluenceCount;
	OnRep_InfluenceCount();	// 리슨 호스트는 OnRep 을 받지 않는다
}

void UCSMeshAffectedByGravityCore::RemoveInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	if (InfluenceCount <= 0) return;

	--InfluenceCount;
	OnRep_InfluenceCount();
}

void UCSMeshAffectedByGravityCore::OnRep_InfluenceCount()
{
	const bool bNowActive = (InfluenceCount > 0);
	if (bNowActive == bBroadcastActive) return;

	bBroadcastActive = bNowActive;

	if (bNowActive)
	{
		OnInteractionStarted.Broadcast();
	}
	else
	{
		OnInteractionEnded.Broadcast();
	}
}

void UCSMeshAffectedByGravityCore::SetEnable( bool bInEnable )
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UStaticMeshComponent* MeshComp = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (!IsValid(MeshComp)) return;

	bEnable = bInEnable;

	MeshComp->SetCollisionObjectType(
		bEnable ? CCHANNEL_CSGRAVITY_CORE_AFFECTED : ECC_WorldDynamic
	);

	MeshComp->SetGenerateOverlapEvents(true);
	MeshComp->UpdateOverlaps();
}

