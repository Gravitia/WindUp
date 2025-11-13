// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CSSwitchBase.h"
#include "Engine/Engine.h"
#include "CSMoveObjectSwitch.generated.h"

USTRUCT(BlueprintType)
struct FMoveObjectData
{
	GENERATED_BODY()

	// 이동시킬 Actor 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
	TObjectPtr<AActor> TargetActor;

	// === 위치 관련 ===
	// 로컬 좌표 기준 이동 오프셋 (상대적 이동량)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Location",
		meta = (DisplayName = "Local Movement Offset (X, Y, Z)"))
	FVector LocalMovementOffset;

	// 초기 위치 (런타임에 설정됨)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move Object|Location")
	FVector InitialLocation;

	// 계산된 목표 위치 (초기 위치 + 오프셋)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move Object|Location")
	FVector CalculatedTargetLocation;

	// === 회전 관련 ===
	// 로컬 회전 오프셋 (상대적 회전량)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Rotation",
		meta = (DisplayName = "Local Rotation Offset (Pitch, Yaw, Roll)"))
	FRotator LocalRotationOffset;

	// 초기 회전 (런타임에 설정됨)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move Object|Rotation")
	FRotator InitialRotation;

	// 계산된 목표 회전 (초기 회전 + 오프셋)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move Object|Rotation")
	FRotator CalculatedTargetRotation;

	// === 설정 옵션 ===
	// 위치 이동 활성화
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Settings")
	bool bEnableLocationMovement;

	// 회전 활성화
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Settings")
	bool bEnableRotationMovement;

	// 로컬 공간 기준 이동 (true: 액터의 로컬 좌표계 기준, false: 월드 좌표계 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Settings")
	bool bUseActorLocalSpace;

	// 스케일 무시 (true: 액터의 스케일을 무시하고 절대 거리 이동)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object|Settings")
	bool bIgnoreScale;

	// 이동 완료 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move Object|Status")
	bool bIsAtTarget;

	FMoveObjectData()
	{
		TargetActor = nullptr;
		LocalMovementOffset = FVector::ZeroVector;
		InitialLocation = FVector::ZeroVector;
		CalculatedTargetLocation = FVector::ZeroVector;
		LocalRotationOffset = FRotator::ZeroRotator;
		InitialRotation = FRotator::ZeroRotator;
		CalculatedTargetRotation = FRotator::ZeroRotator;
		bEnableLocationMovement = true;
		bEnableRotationMovement = true;
		bUseActorLocalSpace = true;  // 기본적으로 로컬 공간 사용
		bIgnoreScale = true;  // 기본적으로 스케일 무시
		bIsAtTarget = false;
	}
};

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSMoveObjectSwitch : public ACSSwitchBase
{
	GENERATED_BODY()
	
public:
	ACSMoveObjectSwitch();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Interact() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetInteracted(bool bInInteracted);

protected:
	// 이동 관련 네트워크 함수들
	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastStartMovement(bool bMoveToTarget);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastUpdateMovement(const TArray<FVector>& NewLocations, const TArray<FRotator>& NewRotations);

	// 이동 로직
	void StartMovement();
	void UpdateMovement(float DeltaTime);
	void InitializeObjectPositions();

	// 목표 위치/회전 계산
	void CalculateTargetTransforms();

protected:
	// 이동시킬 오브젝트들의 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	TArray<FMoveObjectData> MoveObjects;

	// 이동 속도 (유닛/초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed;

	// 회전 속도 (도/초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed;

	// 이동 완료 허용 오차
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	float MoveTolerance;

	// 회전 완료 허용 오차 (도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	float RotationTolerance;

	// 현재 이동 중인지 여부
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	bool bIsMoving;

	// 이동 방향 (true: 목표로, false: 초기 위치로)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Move Object", meta = (AllowPrivateAccess = "true"))
	bool bMovingToTarget;
};
