// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "CSGA_ProjectileBlackHole.generated.h"

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
	// 가이드라인 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float GuideDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float MaxGuideDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float UpdateRate = 0.02f;

	/** 플레이어 전방으로부터 얼마나 떨어진 위치에서 시작할지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float StartOffsetDistance = 100.f;

	// 마우스 Y축 민감도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	float MouseYSensitivity = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Black Hole")
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
	// 마우스 조준 상태 관리
	bool bUsingMouseAiming = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	float MouseMovementThreshold = 1.0f; // 마우스 이동 감지 임계값

private:
	// 초기 조준 방향 저장 (플레이어 회전에 영향받지 않음)
	FVector InitialAimDirection = FVector::ZeroVector;
	bool bInitialDirectionSet = false;

private:
	void CheckMouseMovement();

	// Black Hole 
protected:
	FVector CurrentEndLocation;

	void CheckMouseInput();
	void CreateBlackHoleAtLocation(const FVector& Location);

protected:
	void SpawnBlackHoleDummy(FVector SpawnLocation);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Black Hole Dummy")
	TSubclassOf<class ACSBlackHoleDummy> BlackHoleDummyClass;

	UPROPERTY()
	TObjectPtr<class ACSBlackHoleDummy> BlackHoleDummyActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Black Hole")
	TSubclassOf<class ACSBlackHole> BlackHoleClass;

	UPROPERTY()
	TObjectPtr<class ACSBlackHole> BlackHoleActor;

	bool bIsSpawned;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Check Component")
	bool bCheckMeshComponentPulledByBlackHole = false;
};
