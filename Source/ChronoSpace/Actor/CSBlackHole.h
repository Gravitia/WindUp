// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/CSAbilitySource.h"
#include "CSBlackHole.generated.h"

UCLASS()
class CHRONOSPACE_API ACSBlackHole : public AActor, public ICSAbilitySource
{
	GENERATED_BODY()

public:
	ACSBlackHole();

	// ICSAbilitySource
	virtual ECSAbilityType GetAbilityType() const override { return ECSAbilityType::Blackhole; }

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetDuration(float Duration);
	void SetGravityInfluenceRange(float Range);
	void SetStopRange(float Range);
	void SetPullStrength(float Strength);
	void SetCheckComponentInMesh(bool bCheckComponent);

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnStopTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnStopTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual void Destroyed() override;

	void ProcessForCharacter(float DeltaTime);
	void ProcessForStaticMesh(float DeltaTime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> GravitySphereTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> EventHorizonSphereTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<class UStaticMeshComponent> CoreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<class UStaticMeshComponent> FieldMesh;

	UPROPERTY()
	TSet< TWeakObjectPtr<ACharacter> > CharactersInSphereTrigger;

	UPROPERTY()
	TSet< TWeakObjectPtr<ACharacter> > CharactersInEventHorizon;

	UPROPERTY()
	TSet< TWeakObjectPtr<UStaticMeshComponent> > StaticMeshesInSphereTrigger;

	UPROPERTY()
	TSet< TWeakObjectPtr<UStaticMeshComponent> > StaticMeshesInEventHorizon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
	float GravityInfluenceRange = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
	float PullStrength = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Gravity")
	float StopRadius = 150.0f;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
	float MeshRadius = 50.0f;

	bool bCheckMeshHaveComponent = false;

	/* Sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* BlackHoleOnSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* BlackHoleOffSound;
};
