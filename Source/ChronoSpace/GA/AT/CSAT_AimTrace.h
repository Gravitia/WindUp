// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "CSAT_AimTrace.generated.h"

/** 매 갱신마다 (조준 방향, 트레이스 끝점) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSAimTraceUpdated, FVector, Direction, FVector, EndLocation);

/**
 * 수렴 조준. 카메라에서 뷰 중앙(마우스 Y 반영) 방향으로 트레이스해 "바라보는 지점" 을 구하고,
 * 아바타 눈높이에서 그 지점을 향하는 방향과, 그 방향으로 다시 트레이스한 끝점을
 * UpdateInterval 마다 OnAimUpdated 로 내보낸다. 클라(로컬 플레이어)에서만 의미 있다.
 *
 * 카메라와 눈의 위치 차이(붐 오프셋, 붐 길이, 리그 모디파이어)가 어떻게 바뀌어도 화면에서 보는 곳과 일치한다.
 * 뷰 중앙은 LocalPlayer 의 Origin/Size 에서 읽으므로 분할 좌/우·전환·풀스크린도 자동으로 맞는다.
 * 마우스 위치를 못 얻으면 Activate 시점의 아바타 전방을 쓴다.
 *
 * 시작점·트레이스는 static 으로 열어 두어 서버 쪽 어빌리티가 같은 계산을 쓴다.
 */
UCLASS()
class CHRONOSPACE_API UCSAT_AimTrace : public UAbilityTask
{
	GENERATED_BODY()

public:
	UCSAT_AimTrace();

	static UCSAT_AimTrace* CreateAimTraceTask(UGameplayAbility* OwningAbility, float InMaxDistance, float InMouseYSensitivity, float InUpdateInterval);

	UPROPERTY(BlueprintAssignable)
	FCSAimTraceUpdated OnAimUpdated;

	/** 아바타 눈높이 */
	static FVector GetAimStartLocation(const AActor* Avatar);

	/**
	 * 눈높이에서 Direction 으로 MaxDistance 만큼 라인 트레이스한 끝점.
	 * 아바타와 블랙홀에 끌려가는 액터(CSManagedActorSubsystem)는 무시한다.
	 */
	static FVector TraceAimEnd(const AActor* Avatar, const FVector& Direction, float MaxDistance);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	/** 눈에서 "카메라가 바라보는 지점" 을 향하는 방향 */
	FVector ComputeAimDirection(const AActor* Avatar, UWorld* World) const;

	/** 아바타와 블랙홀에 끌려가는 액터를 무시하는 Visibility 라인 트레이스 */
	static bool LineTraceIgnoringAvatar(UWorld* World, const AActor* Avatar, const FVector& Start, const FVector& End, FHitResult& OutHit);

	float MaxDistance = 2000.f;
	float MouseYSensitivity = 1.f;
	float UpdateInterval = 0.02f;
	float Accumulated = 0.f;

	/** 마우스 위치를 못 얻을 때의 방향 (Activate 시점 아바타 전방) */
	FVector FallbackDirection = FVector::ForwardVector;
};
