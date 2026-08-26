// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSSplineTrack.generated.h"

class USplineComponent;
class ACSSplineRider;

/**
 * 레일의 특정 스플라인 포인트 하나에 대한 트리거 설정.
 * 라이더(ACSSplineRider)가 이 포인트에 도달하면 TargetActors 에 통지한다.
 */
USTRUCT(BlueprintType)
struct FCSSplineTriggerPoint
{
	GENERATED_BODY()

	// 이벤트가 발생할 스플라인 포인트 인덱스.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|SplineTrack|Trigger")
	int32 PointIndex = 0;

	// 도달 시 라이더가 능력 면역 + 위치 고정되는 시간(초). 0 이면 고정 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|SplineTrack|Trigger", meta = (ClampMin = "0.0"))
	float LockDuration = 3.0f;

	// true = 라이더가 포인트를 떠나면 타겟에 false 를 다시 통지 (Toggle).
	// false = 한번 발동하면 유지 (Latch).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|SplineTrack|Trigger")
	bool bDeactivateOnLeave = false;

	// 도달 시 통지할 타겟들. ICSReactorTarget 구현 필요 (문/다리/리액터 등).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CSEditable|SplineTrack|Trigger")
	TArray<TObjectPtr<AActor>> TargetActors;
};

/**
 * 블랙홀/중력코어 능력으로 움직이는 오브젝트(ACSSplineRider)가 따라가는 전용 레일.
 *
 * 레벨에 배치하고 스플라인 포인트를 편집해 경로를 만든다.
 * TriggerPoints 에 "몇 번 포인트에 도달하면 어떤 액터들에 통지할지"를 리스트로 등록한다.
 * 통지는 기존 리액터 시스템과 동일하게 ICSReactorTarget::OnReactorTriggerChanged 로 나간다.
 * (서버/클라 양쪽에서 호출되므로 타겟 BP는 연출을 로컬에서 바로 실행해도 된다)
 *
 * 같은 레일 위 라이더끼리의 점유/밀어내기 판정(ResolveRiderMove)도 여기서 처리한다.
 * 판정은 3D 콜리전이 아니라 스플라인상 거리(1D) 기준이다.
 */
UCLASS()
class CHRONOSPACE_API ACSSplineTrack : public AActor
{
	GENERATED_BODY()

public:
	ACSSplineTrack();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	USplineComponent* GetSpline() const { return Spline; }

	UFUNCTION(BlueprintPure, Category = "SplineTrack")
	float GetDistanceAtPoint(int32 PointIndex) const;

	const TArray<FCSSplineTriggerPoint>& GetTriggerPoints() const { return TriggerPoints; }

	// ==== 라이더 등록 (서버 전용, ACSSplineRider 가 BeginPlay/EndPlay 에서 호출) ====
	void RegisterRider(ACSSplineRider* Rider);
	void UnregisterRider(ACSSplineRider* Rider);

	// 서버 전용: Mover 가 DesiredDistance 로 이동하려 할 때 같은 레일의 다른 라이더와의
	// 점유를 판정해 실제 도달 가능한 거리를 반환한다. 밀 수 있으면(우선순위 크거나 같음)
	// 상대를 연쇄적으로 밀고, 아니면 상대 앞에서 막힌다. 고정(Lock) 중인 라이더는 밀리지 않는다.
	float ResolveRiderMove(ACSSplineRider* Mover, float DesiredDistance);

	// ==== 트리거 통지 (서버 전용, 라이더가 호출) ====
	void NotifyRiderReachedPoint(int32 TriggerEntryIndex);
	void NotifyRiderLeftPoint(int32 TriggerEntryIndex);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_ActivePointsMask();

	// 서버/클라 양쪽: 마스크 변경분(NotifiedMask 와의 XOR)만큼 타겟들에 통지한다.
	void HandleMaskChanged();
	void NotifyTargets(const TArray<TObjectPtr<AActor>>& Targets, bool bActive);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SplineTrack")
	TObjectPtr<USplineComponent> Spline;

	// 이벤트 포인트 목록. 한 레일에 여러 개 등록 가능. (비트마스크 리플리케이션 한계로 최대 32개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|SplineTrack")
	TArray<FCSSplineTriggerPoint> TriggerPoints;

	// 각 TriggerPoints 항목의 활성 상태 비트마스크 (리플리케이트).
	UPROPERTY(ReplicatedUsing = OnRep_ActivePointsMask)
	int32 ActivePointsMask = 0;

	// 마지막으로 타겟에 통지한 마스크. 서버/클라 각자 로컬로 diff 용.
	int32 NotifiedMask = 0;

	// 서버 전용: 이 레일에 올라간 라이더들.
	TArray<TWeakObjectPtr<ACSSplineRider>> Riders;
};
