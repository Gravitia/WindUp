// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSBlackHoleDummy.generated.h"

UCLASS()
class CHRONOSPACE_API ACSBlackHoleDummy : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACSBlackHoleDummy();

	void SetGravityInfluenceRange(float Range);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> Core;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> Field;

	float MeshRadius = 50.0f;
};
