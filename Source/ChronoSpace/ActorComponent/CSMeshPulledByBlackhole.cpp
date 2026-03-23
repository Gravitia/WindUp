// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "ChronoSpace.h"

UCSMeshPulledByBlackhole::UCSMeshPulledByBlackhole()
{

}

void UCSMeshPulledByBlackhole::BeginPlay()
{
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
	if (!IsValid(GetWorld()) || !IsValid(GetOwner())) return;

	UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem< UCSManagedActorSubsystem >();

	if ( IsValid(Subsystem) )
	{
		Subsystem->UnRegisterActorPulledByBlackHole( GetOwner() );
	}
}

void UCSMeshPulledByBlackhole::NotifyInteractionStarted()
{
	OnInteractionStarted.Broadcast();
}

void UCSMeshPulledByBlackhole::NotifyInteractionEnded()
{
	OnInteractionEnded.Broadcast();
}


