// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSFireWindmill.generated.h"

UENUM(BlueprintType)
enum class ECSWindmillAxis : uint8
{
	Pitch,
	Yaw,
	Roll
};

/**
 * 불 풍차 - 계속 도는 날개에 킬존이 붙어 있는 장애물.
 *
 * 날개 메시, 불꽃 나이아가라, 킬존은 블루프린트 자식에서 붙인다.
 * 이 클래스는 액터를 통째로 돌리는 일만 하고, 붙어 있는 것들은 따라 돈다.
 *
 * 회전은 DeltaTime 을 누적하지 않고 서버 기준 시간에서 매번 절대 각도를 구한다.
 * 누적하면 프레임레이트 차이와 히치 때문에 머신마다 각도가 갈라지고,
 * 그러면 클라이언트에서 킬존이 서버와 다른 자리에 있게 된다.
 * (ACSRotatingActor / ACSAnimatedTrap / ACSTransformMover 와 같은 방식)
 */
UCLASS()
class CHRONOSPACE_API ACSFireWindmill : public AActor
{
	GENERATED_BODY()

public:
	ACSFireWindmill();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	/** 블루프린트가 날개/불꽃/킬존을 붙이는 지점 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 초당 회전 각도. 원본 블루프린트는 3초에 90도라 평균 30도/초였다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill", meta = (ClampMin = "0.0"))
	float RotationSpeed = 30.0f;

	/** 어느 축으로 돌지. 원본은 Pitch 로 돌았다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill")
	ECSWindmillAxis RotationAxis = ECSWindmillAxis::Pitch;

	/**
	 * 켜면 원본 블루프린트처럼 끊어 돈다.
	 * StepInterval 주기로 StepSweepTime 동안 StepAngle 만큼 훑고 나머지 시간은 멈춰 있는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill|Stepped")
	bool bSteppedRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill|Stepped",
		meta = (EditCondition = "bSteppedRotation", ClampMin = "0.0"))
	float StepAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill|Stepped",
		meta = (EditCondition = "bSteppedRotation", ClampMin = "0.01"))
	float StepInterval = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|FireWindmill|Stepped",
		meta = (EditCondition = "bSteppedRotation", ClampMin = "0.01"))
	float StepSweepTime = 1.0208f;

private:
	/** 레벨에 배치된 회전. 여기서부터 각도를 더해 나간다. */
	FRotator InitialRotation = FRotator::ZeroRotator;

	/** GameState 기준 시간. GameState 가 없으면 로컬 월드 시간으로 떨어진다. */
	float GetSynchronizedWorldTime() const;

	/** 설정된 축에 각도를 실어 로테이터로 만든다. */
	FRotator MakeSpinRotator(float Angle) const;
};
