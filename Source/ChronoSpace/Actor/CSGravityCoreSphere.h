// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CSGravityCore.h"
#include "Interface/CSAbilitySource.h"
#include "CSGravityCoreSphere.generated.h"

/**
 *
 */
/** 코어 영향을 받기 전의 물리 값 - 벗어날 때 그대로 되돌리기 위해 저장한다 */
struct FCSGravityCoreMeshState
{
	float MaxAngularVelocityRad = 0.f;
	float AngularDamping = 0.f;
	bool bGravityEnabled = true;
};

UCLASS()
class CHRONOSPACE_API ACSGravityCoreSphere : public ACSGravityCore, public ICSAbilitySource
{
	GENERATED_BODY()

public:
	ACSGravityCoreSphere();

	// ICSAbilitySource
	virtual ECSAbilityType GetAbilityType() const override { return ECSAbilityType::GravityCore; }

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

	// 진입 시 저장해 둔 원래 물리 값 (서버 전용)
	TMap< TWeakObjectPtr<UStaticMeshComponent>, FCSGravityCoreMeshState > SavedMeshStates;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float GravityInfluenceRange = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float MeshRadius;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float PullStrength = 4000.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float PullDamping = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Sphere")
	float MaxPullSpeed = 1200.0f;

	bool bCheckMeshHaveComponent = false;
};
