// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "CSGA_ProjectileBlackHole.generated.h"

class UCameraComponent;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_ProjectileBlackHole : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCSGA_ProjectileBlackHole();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float GuideDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float MaxGuideDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float UpdateRate = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float StartOffsetDistance = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float MouseYSensitivity = 1.5f;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Black Hole")
	float Duration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Black Hole")
	float GravityInfluenceRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Black Hole")
	float PullStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Black Hole")
	float StopRange;

private:
	FTimerHandle UpdateTimerHandle;
	FTimerHandle DurationTimerHandle;

	void UpdateGuideLine();
	void OnGuideDurationEnd();
	FVector GetScreenCenterDirection() const;
	FVector GetStartLocation() const;

private:
	bool bUsingMouseAiming = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	float MouseMovementThreshold = 1.0f; 

private:
	FVector InitialAimDirection = FVector::ZeroVector;
	bool bInitialDirectionSet = false;

private:
	void CheckMouseMovement();

	// Black Hole 
protected:
	FVector CurrentEndLocation;
	FVector CurrentDirection;    

	void CheckMouseInput();
	void CreateBlackHoleAtLocation(const FVector& Direction);

protected:
	//void SpawnBlackHoleDummy(FVector SpawnLocation);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Black Hole Dummy")
	TSubclassOf<class ACSBlackHoleDummy> BlackHoleDummyClass;

	UPROPERTY()
	TObjectPtr<class ACSBlackHoleDummy> BlackHoleDummyActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Black Hole")
	TSubclassOf<class ACSBlackHole> BlackHoleClass;

	//bool bIsDummySpawned;
	bool bIsBlackHoleSpawned;

protected:
	bool bCheckMeshComponentPulledByBlackHole{ true };

public:
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	bool bIsAming;

protected:
	/** 블랙홀 조준 시 사용하는 카메라 줌 Ability (BP 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSubclassOf<UGameplayAbility> CameraZoomAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Camera")
	float CameraZOffsetWhileAiming = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Camera")
	bool bApplyCameraZOffsetWhileAiming = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Camera")
	float CameraOffsetLerpDuration = 0.5f;

	/** 줌 복원(줌아웃) Lerp 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Camera")
	float CameraOffsetRestoreLerpDuration = 0.3f;

	/**
	 * 카메라 오프셋의 적용/복원과 원본값 보관은 전부 UCSCameraRigComponent 가 한다.
	 * 어빌리티가 타이머로 Lerp 를 돌리면 UGameplayAbility::EndAbility 의
	 * ClearAllTimersForObject(this) 에 지워져 복원이 아예 실행되지 않는다.
	 */
	void ApplyCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo);
	void RestoreCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo);

	class ACSCharacterPlayer* GetCameraRigOwner(const FGameplayAbilityActorInfo* ActorInfo) const;
};
