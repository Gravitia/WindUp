// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSSplineRider.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ACSSplineTrack;

/**
 * 트리거 포인트 고정(Lock)이 풀린 뒤의 동작.
 *  - Normal  : 마지막 위치에 그대로 남는다.
 *  - Recover : 능력이 닿지 않는 동안 RecoverSpeed 로 시작 포인트로 복귀한다.
 *              복귀 중에도 능력으로 다시 끌 수 있다.
 */
UENUM(BlueprintType)
enum class ECSSplineRiderMoveType : uint8
{
	Normal   UMETA(DisplayName = "Normal"),
	Recover  UMETA(DisplayName = "Recover"),
};

/**
 * 레일(ACSSplineTrack)에 구속되어 블랙홀/중력코어 능력의 당김으로만 움직이는 오브젝트.
 *
 * 이동은 물리 시뮬레이션이 아니라 "스플라인상 거리(1D)" 하나로 계산한다.
 * 서버가 매 틱 영향권 안의 능력 소스(ICSAbilitySource)들의 당김을 스플라인 접선에
 * 투영해 거리를 갱신하고, RepDistance 리플리케이션으로 클라이언트가 보간해 따라온다.
 * 회전은 건드리지 않는다 (위치만 스플라인 반영).
 *
 * 트리거 포인트 도달 시: 레일에 통지 → LockDuration 동안 능력 면역 + 위치 고정 →
 * 해제 후 MoveType 에 따라 제자리 유지(Normal) 또는 시작점 복귀(Recover).
 *
 * 같은 레일의 다른 라이더와는 1D 점유 판정으로 막히거나(PushPriority 낮음)
 * 밀어낸다(PushPriority 크거나 같음). 예: 블랙홀용 라이더 1 / 중력코어용 라이더 0
 * 으로 두면 블랙홀 쪽이 중력코어 쪽을 밀 수 있고 반대는 막힌다.
 *
 * 기획자는 이 클래스를 상속한 BP에서 메시/수치를 세팅하고,
 * OnRailLockStarted / OnRailLockEnded 에 이미시브·사운드 연출만 구현하면 된다.
 */
UCLASS(Abstract, Blueprintable)
class CHRONOSPACE_API ACSSplineRider : public AActor
{
	GENERATED_BODY()

public:
	ACSSplineRider();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	// ==== 레일(ACSSplineTrack)이 점유 판정에 쓰는 접근자 ====
	float GetRailDistance() const { return RepDistance; }
	float GetRailBlockRadius() const { return RailBlockRadius; }
	int32 GetPushPriority() const { return PushPriority; }

	UFUNCTION(BlueprintPure, Category = "SplineRider")
	bool IsRailLocked() const { return bRailLocked; }

	// 서버 전용: 다른 라이더에게 밀렸을 때 레일이 호출한다.
	void ApplyPushedDistance(float NewDistance, float InheritVelocity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ==== 기획자가 BP에서 구현하는 연출 훅 (서버/클라 양쪽에서 호출) ====
	// 트리거 포인트에 고정되는 순간. 이미시브 발광/사운드 연출용.
	UFUNCTION(BlueprintImplementableEvent, Category = "SplineRider", meta = (DisplayName = "On Rail Lock Started"))
	void OnRailLockStarted();

	// 고정이 풀리는 순간. 이미시브 소거용.
	UFUNCTION(BlueprintImplementableEvent, Category = "SplineRider", meta = (DisplayName = "On Rail Lock Ended"))
	void OnRailLockEnded();

protected:
	UFUNCTION()
	void OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// OtherActor 가 ICSAbilitySource 이고 RespondsToAbilities 에 포함된 능력인지. (리액터와 동일 규칙)
	bool IsAbilitySourceOfInterest(const AActor* OtherActor) const;

	// 서버 전용: 한 틱의 이동 처리 (당김 → 점유 판정 → 거리 적용).
	void ServerMove(float DeltaSeconds);

	// 서버 전용: 거리 확정 + 액터 위치 반영 + 트리거 포인트 도달/이탈 판정.
	void SetServerDistance(float NewDistance);

	// 서버 전용: 트리거 포인트 도달 처리 (레일 통지 + Lock 시작).
	void DockAtTriggerPoint(int32 TriggerEntryIndex);
	void EndRailLock();

	void SetRailLocked(bool bNewLocked);
	void HandleRailLockChanged();

	UFUNCTION()
	void OnRep_RepDistance();

	UFUNCTION()
	void OnRep_RailLocked();

	// 현재 거리 기준 액터 위치를 스플라인에 스냅한다. 회전은 유지.
	void ApplyLocationAtDistance(float Distance);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SplineRider")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 능력 소스(블랙홀/중력코어) 감지용. 소스의 영향 범위 스피어와 오버랩되면 당김을 받는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SplineRider")
	TObjectPtr<USphereComponent> AbilityDetection;

	// ==== 배치 설정 ====
	// 이 라이더가 따라갈 레일. 레벨에 배치된 ACSSplineTrack 중 선택.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SplineRider")
	TObjectPtr<ACSSplineTrack> TargetTrack;

	// 게임 시작 시 스플라인의 몇 번 포인트에서 시작할지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider")
	int32 StartPointIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider")
	ECSSplineRiderMoveType MoveType = ECSSplineRiderMoveType::Normal;

	// MoveType 이 Recover 일 때 시작점으로 복귀하는 속도 (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider", meta = (ClampMin = "0.0", EditCondition = "MoveType == ECSSplineRiderMoveType::Recover"))
	float RecoverSpeed = 200.0f;

	// 어떤 능력에 반응할지. 아무것도 체크하지 않으면(0) 모든 능력에 반응.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider",
		meta = (Bitmask, BitmaskEnum = "/Script/ChronoSpace.ECSAbilityType"))
	int32 RespondsToAbilities = 0;

	// ==== 이동 튜닝 ====
	// 능력 소스 하나가 주는 당김 가속 (cm/s^2). 접선 투영 후 적용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Move", meta = (ClampMin = "0.0"))
	float PullAcceleration = 1500.0f;

	// 속도 감쇠 계수. 클수록 능력이 떨어졌을 때 빨리 멈춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Move", meta = (ClampMin = "0.0"))
	float MoveDamping = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Move", meta = (ClampMin = "0.0"))
	float MaxMoveSpeed = 600.0f;

	// 레일 경사를 따라 미끄러지는 중력 가속 배율. 접선의 상하 성분에 월드 중력을 곱해 적용한다.
	// 1 = 실제 중력, 0 = 경사 영향 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Move", meta = (ClampMin = "0.0"))
	float RailGravityScale = 1.0f;

	// ==== 점유 / 밀어내기 ====
	// 1D 점유 판정 반경. 다른 라이더와 이 반경의 합 이하로는 겹치지 못한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Push", meta = (ClampMin = "0.0"))
	float RailBlockRadius = 60.0f;

	// 상대보다 크거나 같으면 밀 수 있고, 작으면 상대 앞에서 막힌다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Push")
	int32 PushPriority = 0;

	// ==== 트리거 포인트 ====
	// 포인트 도달로 판정하는 거리 허용 오차 (cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider", meta = (ClampMin = "1.0"))
	float ArriveTolerance = 25.0f;

	// 감지 스피어 반경.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider", meta = (ClampMin = "0.0"))
	float DetectionRadius = 100.0f;

	// 클라이언트 보간 속도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider", meta = (ClampMin = "0.0"))
	float InterpSpeed = 10.0f;

	// 켜면 당김/점유/트리거 판정을 로그로 출력한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SplineRider|Debug")
	bool bRiderDebug = false;

protected:
	// 스플라인상 거리 (리플리케이트). 서버가 권한.
	UPROPERTY(ReplicatedUsing = OnRep_RepDistance)
	float RepDistance = 0.0f;

	// 트리거 포인트 고정(능력 면역) 상태 (리플리케이트).
	UPROPERTY(ReplicatedUsing = OnRep_RailLocked)
	bool bRailLocked = false;

	// 서버 전용: 현재 영향권에 들어온 능력 소스들.
	TSet<TWeakObjectPtr<AActor>> ActiveSources;

	// 서버 전용: 1D 속도 (cm/s, 스플라인 정방향 기준 부호).
	float RailVelocity = 0.0f;

	// 서버 전용: StartPointIndex 의 거리. Recover 복귀 목표.
	float StartDistance = 0.0f;

	// 서버 전용: 현재 도킹 중인 TriggerPoints 인덱스. 떠나기 전까지 재도달 판정을 막는다.
	int32 DockedEntryIndex = INDEX_NONE;

	FTimerHandle LockTimerHandle;

	// 클라이언트 보간용.
	float TargetDistance = 0.0f;
	float SmoothedDistance = 0.0f;
};
