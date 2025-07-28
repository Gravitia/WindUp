// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "CSGA_ProjectileGuide.generated.h"

class UCameraComponent;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSGA_ProjectileGuide : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UCSGA_ProjectileGuide();
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

	// 블랙홀 어빌리티 클래스 설정 (에디터에서 변경 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Guide")
	TSubclassOf<class UCSGA_BlackHole> BlackHoleAbilityClass;

	// 현재 EndLocation 저장
	FVector CurrentEndLocation;

	// 새로운 함수들
	void CheckMouseInput();
	void CreateBlackHoleAtLocation(const FVector& Location);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	EAbilityIndex Ability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	TSubclassOf<class UGameplayAbility> AbilityClass;

};
