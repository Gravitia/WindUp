// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

UCSMeshPulledByBlackhole::UCSMeshPulledByBlackhole()
{
	// InfluenceCount 를 클라로 보내기 위해 필요 (오너가 복제되지 않는 액터면 자연히 무시된다)
	SetIsReplicatedByDefault(true);
}

void UCSMeshPulledByBlackhole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCSMeshPulledByBlackhole, InfluenceCount);
}

void UCSMeshPulledByBlackhole::BeginPlay()
{
	Super::BeginPlay();

	if ( !IsValid(GetWorld()) || !IsValid(GetOwner()) ) return;
	UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem< UCSManagedActorSubsystem >();

	if ( IsValid( Subsystem ) )
	{
		UE_LOG(LogCS, Log, TEXT("Actor Registered"));
		Subsystem->RegisterActorPulledByBlackHole(GetOwner());
	}
}

void UCSMeshPulledByBlackhole::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(GetWorld()) && IsValid(GetOwner()))
	{
		UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem<UCSManagedActorSubsystem>();

		if (IsValid(Subsystem))
		{
			Subsystem->UnRegisterActorPulledByBlackHole(GetOwner());
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UCSMeshPulledByBlackhole::AddInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	if (InfluenceCount++ == 0)
	{
		SaveAndApplyAffectedState();
	}

	OnRep_InfluenceCount();	// 리슨 호스트는 OnRep 을 받지 않는다
}

void UCSMeshPulledByBlackhole::RemoveInfluence()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;
	if (InfluenceCount <= 0) return;

	if (--InfluenceCount == 0)
	{
		RestoreAffectedState();
	}

	OnRep_InfluenceCount();
}

void UCSMeshPulledByBlackhole::OnRep_InfluenceCount()
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

void UCSMeshPulledByBlackhole::SaveAndApplyAffectedState()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	AffectedMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (!IsValid(AffectedMesh)) return;

	// 원래 값을 저장한다. 예전엔 블랙홀이 나갈 때 원래 값과 무관하게 Block 으로 되돌려
	// 원래 카메라를 무시하던 장식 메시가 이후 영구히 카메라를 막았다.
	bSavedGravityEnabled = AffectedMesh->IsGravityEnabled();
	SavedCameraResponse = AffectedMesh->GetCollisionResponseToChannel(ECC_Camera);
	bHasSavedState = true;

	AffectedMesh->SetEnableGravity(false);
	AffectedMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void UCSMeshPulledByBlackhole::RestoreAffectedState()
{
	if (!bHasSavedState || !IsValid(AffectedMesh)) return;

	AffectedMesh->SetEnableGravity(bSavedGravityEnabled);
	AffectedMesh->SetCollisionResponseToChannel(ECC_Camera, SavedCameraResponse);
	bHasSavedState = false;
	AffectedMesh = nullptr;
}


