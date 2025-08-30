// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CSGravityCore.h"
#include "CSGravityCoreSphere.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSGravityCoreSphere : public ACSGravityCore
{
	GENERATED_BODY()
	
public:
	ACSGravityCoreSphere();

	FORCEINLINE float GetMeshRadius() { return MeshRadius; }
	FORCEINLINE void SetCheckComponentInMesh(bool bInMeshHaveComponent) { bCheckMeshHaveComponent = bInMeshHaveComponent; }

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void ProcessForStaticMesh(float DeltaTime);

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnCoreBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);


	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> SphereTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> StaticMeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sphere")
	TSoftObjectPtr<class UStaticMesh> StaticMesh;

	UPROPERTY()
	TMap<FName, TObjectPtr<ACharacter> > CharactersInSphereTrigger;

	UPROPERTY()
	TSet< TWeakObjectPtr<UStaticMeshComponent> > StaticMeshesInSphereTrigger;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float GravityInfluenceRange = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float MeshRadius;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float PullStrength = 4000.0f;

	bool bCheckMeshHaveComponent = false;
};
