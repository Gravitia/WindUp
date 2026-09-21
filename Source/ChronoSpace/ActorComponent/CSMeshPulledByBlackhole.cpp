// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "Subsystem/CSCameraOcclusionFadeSubsystem.h"
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

	IgnoreCameraCollision();

	UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem< UCSManagedActorSubsystem >();

	if ( IsValid( Subsystem ) )
	{
		UE_LOG(LogCS, Log, TEXT("Actor Registered"));
		Subsystem->RegisterActorPulledByBlackHole(GetOwner());
	}

	// 카메라 채널을 무시하는 대신 캐릭터를 가릴 수 있으므로 페이드 대상으로도 등록한다
	if (UCSCameraOcclusionFadeSubsystem* Fade = GetWorld()->GetSubsystem<UCSCameraOcclusionFadeSubsystem>())
	{
		Fade->RegisterTarget(GetOwner());
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

		if (UCSCameraOcclusionFadeSubsystem* Fade = GetWorld()->GetSubsystem<UCSCameraOcclusionFadeSubsystem>())
		{
			Fade->UnregisterTarget(GetOwner());
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

	// 원래 값을 저장한다. 블랙홀이 나갈 때 그대로 되돌리기 위해서다.
	bSavedGravityEnabled = AffectedMesh->IsGravityEnabled();
	bHasSavedState = true;

	AffectedMesh->SetEnableGravity(false);
}

void UCSMeshPulledByBlackhole::RestoreAffectedState()
{
	if (!bHasSavedState || !IsValid(AffectedMesh)) return;

	AffectedMesh->SetEnableGravity(bSavedGravityEnabled);
	bHasSavedState = false;
	AffectedMesh = nullptr;
}

void UCSMeshPulledByBlackhole::IgnoreCameraCollision()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	// 블랙홀에 끌리는 동안만 무시하던 것을 상시로 바꿨다.
	// 평소에도 이 오브젝트가 캐릭터를 가리면 스프링암이 줄어들었고,
	// 서버에서만 바꾸던 탓에 클라이언트에서는 끌리는 중에도 카메라가 밀렸다.
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Owner->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh))
		{
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}
}


