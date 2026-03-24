// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/CSAutoTrap.h"
#include "CSAnimatedTrap.generated.h"

/* ─────────────────────────────────────────────
 *  Enums
 * ───────────────────────────────────────────── */

UENUM(BlueprintType)
enum class EAnimTrapType : uint8
{
	Translate  UMETA(DisplayName = "이동"),
	Rotate     UMETA(DisplayName = "회전")
};

UENUM(BlueprintType)
enum class EAnimTrapAxis : uint8
{
	X, Y, Z
};

/* ─────────────────────────────────────────────
 *  하나의 컴포넌트 움직임 설정
 * ───────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct FAnimTrapEntry
{
	GENERATED_BODY()

	/** 움직일 컴포넌트의 태그 (비어 있으면 RootComponent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TargetComponentTag;

	/** 이동 vs 회전 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAnimTrapType AnimType = EAnimTrapType::Translate;

	/** 축 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAnimTrapAxis AnimAxis = EAnimTrapAxis::Z;

	/** 진폭 — Translate: cm, Rotate: degree */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Amplitude = 200.0f;

	/** 왕복 1회 소요 시간 (초) — 이동 구간만의 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Period = 2.0f;

	/** 끝점에서 대기 시간 (초). 0이면 멈춤 없이 연속 진동 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HoldTime = 0.0f;

	/* ── 런타임 캐시 (에디터 비노출) ── */

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedComp = nullptr;

	FVector  InitialLocation = FVector::ZeroVector;
	FRotator InitialRotation = FRotator::ZeroRotator;
};

/**
 * 반복 움직임 트랩 (프레스 판 / 초침 / 압력기)
 *
 *  AnimEntries 배열에 여러 항목을 추가하면
 *  하나의 액터에서 여러 컴포넌트를 각각 다른 설정으로 움직인다.
 *
 *  sin(2π·t/Period) 기반, ServerStartTime만 Replicate.
 */
UCLASS()
class CHRONOSPACE_API ACSAnimatedTrap : public ACSAutoTrap
{
	GENERATED_BODY()

public:
	ACSAnimatedTrap();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	/* ════════════════════════════════════
	 *  에디터 설정
	 * ════════════════════════════════════ */

	/** 움직일 컴포넌트 + 설정 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimTrap")
	TArray<FAnimTrapEntry> AnimEntries;

	/** BeginPlay 시 자동 시작 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AnimTrap")
	bool bAutoStart = true;

	/* ════════════════════════════════════
	 *  동기화
	 * ════════════════════════════════════ */

	UPROPERTY(ReplicatedUsing = OnRep_AnimState)
	bool bAnimActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_AnimState)
	float ServerStartTime = 0.0f;

	UFUNCTION()
	void OnRep_AnimState();

	/* ════════════════════════════════════
	 *  제어 API
	 * ════════════════════════════════════ */

	UFUNCTION(BlueprintCallable, Category = "AnimTrap")
	void StartAnim();

	UFUNCTION(BlueprintCallable, Category = "AnimTrap")
	void StopAnim();

private:
	void ResolveTargetComponents();
	void ApplyAnimation();

	static FVector  GetAxisVector(EAnimTrapAxis Axis);
	static FRotator MakeAxisRotator(EAnimTrapAxis Axis, float Deg);

	/** HoldTime 포함 파형 계산. 반환값 범위 [-1, 1] */
	static float CalcAlpha(float Elapsed, float Period, float HoldTime);
};
