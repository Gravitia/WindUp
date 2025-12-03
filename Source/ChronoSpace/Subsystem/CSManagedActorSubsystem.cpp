// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSManagedActorSubsystem.h"

TArray< TObjectPtr< AActor > > UCSManagedActorSubsystem::GetActorsPulledByBlackHole()
{ 
	return ActorsPulledByBlackHole;
}

void UCSManagedActorSubsystem::RegisterActorPulledByBlackHole( AActor* ActorPulledByBlackHole )
{
	ActorsPulledByBlackHole.Add( ActorPulledByBlackHole );
}

void UCSManagedActorSubsystem::UnRegisterActorPulledByBlackHole(AActor* ActorPulledByBlackHole)
{
	ActorsPulledByBlackHole.Remove( ActorPulledByBlackHole );
}
