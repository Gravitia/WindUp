// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CSManagedActorSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSManagedActorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	TArray< TObjectPtr< AActor > > GetActorsPulledByBlackHole(); 
	void RegisterActorPulledByBlackHole( AActor* ActorPulledByBlackHole );
	void UnRegisterActorPulledByBlackHole( AActor * ActorPulledByBlackHole );

protected:
	TArray< TObjectPtr< AActor > > ActorsPulledByBlackHole;
};
