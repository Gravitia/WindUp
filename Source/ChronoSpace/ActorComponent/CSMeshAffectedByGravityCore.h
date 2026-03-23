// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSMeshAffectedByGravityCore.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGravityCoreInteractionStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGravityCoreInteractionEnded);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSMeshAffectedByGravityCore : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSMeshAffectedByGravityCore();

	virtual void BeginPlay() override;

	void NotifyInteractionStarted();
	void NotifyInteractionEnded();

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnGravityCoreInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnGravityCoreInteractionEnded OnInteractionEnded;
};
