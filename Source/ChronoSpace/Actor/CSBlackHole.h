// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSBlackHole.generated.h"

UCLASS()
class CHRONOSPACE_API ACSBlackHole : public AActor
{
	GENERATED_BODY()
	
public:
	ACSBlackHole();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetDuration(float Duration);
	void SetGravityInfluenceRange(float Range);
	void SetStopRange(float Range);
	void SetPullStrength(float Strength);

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> GravitySphereTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> EventHorizonSphereTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<class UStaticMeshComponent> CoreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<class UStaticMeshComponent> FieldMesh;

	UPROPERTY()
	TMap<FName, TObjectPtr<ACharacter> > CharactersInSphereTrigger;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float GravityInfluenceRange = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float PullStrength = 4000.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float StopRadius = 100.0f;	

	float MeshRadius = 50.0f;
};
