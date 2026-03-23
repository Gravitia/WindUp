// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSMeshPulledByBlackhole.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackholeInteractionStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackholeInteractionEnded);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSMeshPulledByBlackhole : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSMeshPulledByBlackhole();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void NotifyInteractionStarted();
	void NotifyInteractionEnded();

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnBlackholeInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnBlackholeInteractionEnded OnInteractionEnded;
};
