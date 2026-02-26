// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSConveyorPlatform.generated.h"

class UStaticMeshComponent;
class ACSConveyorManager;

UCLASS()
class CHRONOSPACE_API ACSConveyorPlatform : public AActor
{
	GENERATED_BODY()
	
public:
	ACSConveyorPlatform();

	virtual void Tick(float DeltaSeconds) override;

	void SetManager(ACSConveyorManager* InManager);
	void SetIndexOffset(float InOffsetDistance);

	float GetOffsetDistance() const { return OffsetDistance; }

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Conveyor|Offset")
	float ZOffset = 0.f;

private:
	UPROPERTY()
	ACSConveyorManager* Manager = nullptr;

	// Manager가 (index * spacing)으로 세팅
	float OffsetDistance = 0.f;

};
