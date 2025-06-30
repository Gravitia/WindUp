// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CSKillZone.generated.h"

UENUM(BlueprintType)
enum class EKillZoneType : uint8
{
	Fall,
	Lava,
	Water,
	Toxic,
	Laser,
	Void
};

UCLASS()
class CHRONOSPACE_API ACSKillZone : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACSKillZone();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
