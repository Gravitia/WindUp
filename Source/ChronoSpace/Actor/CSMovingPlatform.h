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

	UPROPERTY(VisibleAnywhere, Category = "Default|LevelDesign")
	UStaticMeshComponent* Mesh;

	// Assign in level per platform instance
	UPROPERTY(EditAnywhere, Category = "Default|LevelDesign")
	ACSTrackManager* Track = nullptr;

	// cm offset along spline
	UPROPERTY(EditAnywhere, Category = "Default|LevelDesign")
	float OffsetDistance = 0.f;

	// optional vertical offset
	UPROPERTY(EditAnywhere, Category = "Default|LevelDesign")
	float ZOffset = 0.f;
};
