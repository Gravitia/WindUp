// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSFireWindmill.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

ACSFireWindmill::ACSFireWindmill()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 회전은 각 머신이 서버 시간에서 직접 구한다.
	// 트랜스폼까지 복제하면 복제된 값과 계산한 값이 서로 덮어써서 떨린다.
	SetReplicateMovement(false);
}

void ACSFireWindmill::BeginPlay()
{
	Super::BeginPlay();

	InitialRotation = GetActorRotation();
}

float ACSFireWindmill::GetSynchronizedWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.0f;
}

FRotator ACSFireWindmill::MakeSpinRotator(float Angle) const
{
	switch (RotationAxis)
	{
	case ECSWindmillAxis::Yaw:
		return FRotator(0.0f, Angle, 0.0f);
	case ECSWindmillAxis::Roll:
		return FRotator(0.0f, 0.0f, Angle);
	default:
		return FRotator(Angle, 0.0f, 0.0f);
	}
}

void ACSFireWindmill::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Time = FMath::Max(GetSynchronizedWorldTime(), 0.0f);

	float Angle = 0.0f;

	if (bSteppedRotation)
	{
		// 원본 블루프린트의 움직임: StepInterval 마다 StepSweepTime 동안 0 -> StepAngle 을 훑고 멈춘다.
		// 날개가 4장이라 한 스텝이 끝나고 처음 각도로 돌아가도 이어서 도는 것처럼 보인다.
		const float Interval = FMath::Max(StepInterval, UE_KINDA_SMALL_NUMBER);
		const float Sweep = FMath::Max(StepSweepTime, UE_KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp(FMath::Fmod(Time, Interval) / Sweep, 0.0f, 1.0f);
		Angle = StepAngle * Alpha;
	}
	else
	{
		Angle = RotationSpeed * Time;
	}

	SetActorRotation(InitialRotation + MakeSpinRotator(Angle));
}
