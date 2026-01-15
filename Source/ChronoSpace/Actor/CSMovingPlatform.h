// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSMovingPlatform.generated.h"

class UStaticMeshComponent;
class ACSTrackManager;

UCLASS()
class CHRONOSPACE_API ACSMovingPlatform : public AActor
{
	GENERATED_BODY()
	
public:
	ACSMovingPlatform();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Platform")
	UStaticMeshComponent* Mesh;

	// Assign in level per platform instance
	UPROPERTY(EditInstanceOnly, Category = "Platform")
	ACSTrackManager* Track = nullptr;

	// cm offset along spline
	UPROPERTY(EditInstanceOnly, Category = "Platform")
	float OffsetDistance = 0.f;

	// optional vertical offset
	UPROPERTY(EditInstanceOnly, Category = "Platform")
	float ZOffset = 0.f;
};
