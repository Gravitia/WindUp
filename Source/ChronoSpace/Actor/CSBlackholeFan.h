// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CSAbilityReactorBase.h"
#include "CSBlackholeFan.generated.h"

/**
 * 팬의 회전 구간 하나. 시작 시점의 각도/속도에서 목표 속도까지 선형으로 오르내린다.
 *
 * 회전 각도를 프레임마다 누적하지 않고 이 값들로 매 프레임 다시 계산한다.
 * 누적하면 프레임레이트와 히치 때문에 머신마다 각도가 갈라진다.
 * 속도가 변하는 팬이라 "속도 x 시간" 한 방으로는 안 되므로, 구간이 바뀔 때만
 * 이 구조체를 복제하고 각 머신이 같은 식으로 적분한다.
 */
USTRUCT()
struct FCSFanSpinPhase
{
	GENERATED_BODY()

	/** 이 구간이 시작된 서버 기준 시간 */
	UPROPERTY()
	float StartServerTime = 0.f;

	/** 시작 시점까지 누적된 회전 각도 */
	UPROPERTY()
	float StartAngle = 0.f;

	/** 시작 속도 (deg/s) */
	UPROPERTY()
	float StartSpeed = 0.f;

	/** 도달할 속도 (deg/s). 0 이면 정지 */
	UPROPERTY()
	float TargetSpeed = 0.f;

	/** 시작 속도에서 목표 속도까지 걸리는 시간(초). 0 이면 즉시 */
	UPROPERTY()
	float RampTime = 0.f;
};

/** 블랙홀이 붙어 있는 동안 충돌을 꺼 둘 컴포넌트와 원래 설정 */
USTRUCT()
struct FCSFanBlocker
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Component = nullptr;

	/** 되돌릴 때 쓸 원래 값. Block 으로 덮어쓰지 않고 설정한 그대로 복구한다. */
	TEnumAsByte<ECollisionEnabled::Type> OriginalCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
};

/**
 * 블랙홀에 반응해 멈추는 회전 팬.
 *
 * 평소에는 RotationSpeed 로 계속 돈다.
 * 블랙홀 능력이 감지되면 SpinDownTime 동안 서서히 멈추고, 날개의 충돌을 꺼서 지나갈 수 있게 한다.
 * 감지가 풀리면 ResumeDelay 만큼 기다렸다가 SpinUpTime 동안 원래 속도로 되돌아온다.
 *
 * 감지는 부모(ACSAbilityReactorBase)가 전부 처리한다.
 * 어떤 능력에 반응할지는 디테일 패널의 RespondsToAbilities 에서 고른다.
 *
 * 날개 메시와 충돌 컴포넌트는 블루프린트 자식에서 붙인다.
 * 멈출 때 꺼야 하는 충돌 컴포넌트에는 BlockingComponentTag 와 같은 태그를 달아 준다.
 */
UCLASS()
class CHRONOSPACE_API ACSBlackholeFan : public ACSAbilityReactorBase
{
	GENERATED_BODY()

public:
	ACSBlackholeFan();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 지금 회전 속도 (deg/s). 연출이나 사운드에서 쓸 수 있다. */
	UFUNCTION(BlueprintPure, Category = "BlackholeFan")
	float GetCurrentSpinSpeed() const;

protected:
	/** 부모가 활성/비활성을 알릴 때. 여기서 감속/가속 구간을 시작한다. */
	virtual void HandleActivationChanged() override;

	/** 평상시 회전 속도 (deg/s). 원본 블루프린트의 타임라인은 400 이었다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan", meta = (ClampMin = "0.0"))
	float RotationSpeed = 400.f;

	/**
	 * 회전 축. 로컬 기준이고 값이 1 인 성분으로 돈다.
	 * 원본 블루프린트는 Roll 로 돌았다. 반대로 돌리려면 -1 을 넣는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan")
	FRotator SpinAxis = FRotator(0.f, 0.f, 1.f);

	/** 블랙홀이 닿은 뒤 완전히 멈추기까지 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan", meta = (ClampMin = "0.0"))
	float SpinDownTime = 1.f;

	/** 감지가 풀린 뒤 다시 돌기 시작할 때까지 기다리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan", meta = (ClampMin = "0.0"))
	float ResumeDelay = 3.f;

	/** 다시 원래 속도까지 올라가는 데 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan", meta = (ClampMin = "0.0"))
	float SpinUpTime = 1.f;

	/**
	 * 멈춰 있는 동안 충돌을 꺼 둘 컴포넌트의 태그.
	 * 블루프린트에서 날개의 Blocking 컴포넌트에 이 태그를 달아 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|BlackholeFan")
	FName BlockingComponentTag = TEXT("FanBlocking");

	UFUNCTION()
	void OnRep_SpinPhase();

private:
	/** 서버 전용: 새 회전 구간을 시작한다. 지금 각도와 속도를 이어받는다. */
	void BeginSpinPhase(float NewTargetSpeed, float RampTime);

	/** ResumeDelay 가 끝나고 다시 돌기 시작 */
	void StartSpinUp();

	/** 태그가 달린 컴포넌트의 충돌을 켜거나 끈다 */
	void ApplyBlockingCollision(bool bEnabled);

	float EvaluateAngle(float ServerTime) const;
	float EvaluateSpeed(float ServerTime) const;

	/** GameState 기준 시간. 없으면 로컬 월드 시간으로 떨어진다. */
	float GetSynchronizedWorldTime() const;

	/** 레벨에 배치된 회전. 여기에 스핀을 곱한다. */
	FQuat InitialRotation = FQuat::Identity;

	UPROPERTY(ReplicatedUsing = OnRep_SpinPhase)
	FCSFanSpinPhase SpinPhase;

	UPROPERTY()
	TArray<FCSFanBlocker> Blockers;

	FTimerHandle ResumeTimer;
};
