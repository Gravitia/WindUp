// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSTrackManager.generated.h"

class USplineComponent;

UCLASS()
class CHRONOSPACE_API ACSTrackManager : public AActor
{
	GENERATED_BODY()
	
public:
	ACSTrackManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	USplineComponent* GetSpline() const { return Spline; }
	float GetSplineLength() const { return SplineLength; }
	float GetSmoothedDistance() const { return SmoothedDistance; }

protected:
	virtual void BeginPlay() override;

	// Track spline owned by this manager (recommended)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|TrackManager")
	USplineComponent* Spline;

	// cm/s
	UPROPERTY(EditAnywhere, Category = "CSEditable|TrackManager")
	float MoveSpeed = 300.f;

	// smoothing for clients
	UPROPERTY(EditAnywhere, Category = "CSEditable|TrackManager")
	float InterpSpeed = 8.f;

	// replicated progress distance (0..SplineLength)
	UPROPERTY(ReplicatedUsing = OnRep_RepDistance)
	float RepDistance = 0.f;

	UFUNCTION()
	void OnRep_RepDistance();

private:
	float SplineLength = 0.f;

	// client-side
	float TargetDistance = 0.f;
	float SmoothedDistance = 0.f;

};
