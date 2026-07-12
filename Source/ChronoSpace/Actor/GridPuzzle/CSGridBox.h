// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSGridBox.generated.h"

/**
 * 격자 위에서 미는 박스. (PUSH PUSH 테스트용)
 *
 * 캐릭터가 몸으로 밀면(블로킹 히트) 미는 방향의 다음 칸으로 한 칸 이동한다.
 *  - bPushable = false 면 밀어도 움직이지 않는 고정 박스가 된다. (색으로 구분)
 *  - 목표 칸이 다른 박스/벽/캐릭터로 막혀 있거나 보드 밖이면 움직이지 않는다.
 *  - 이동은 서버에서만 판정하고, 위치는 리플리케이션으로 동기화된다.
 *
 * 보드(ACSGridBoard)를 지정하지 않으면 BeginPlay 에서 가장 가까운 보드를 자동으로 찾고,
 * 보드 위에 있으면 칸 중심에 스냅된다.
 */
UCLASS()
class CHRONOSPACE_API ACSGridBox : public AActor
{
	GENERATED_BODY()

public:
	ACSGridBox();

	virtual void Tick(float DeltaSeconds) override;

	void SetBoard(class ACSGridBoard* InBoard) { Board = InBoard; }

	UFUNCTION(BlueprintPure, Category = "GridBox")
	bool IsPushable() const { return bPushable; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 서버 전용. 캐릭터가 미는 방향으로 한 칸 이동을 시도한다.
	void TryPush(class ACharacter* Pusher);

	// 목표 위치에 박스가 들어갈 자리가 비어 있는지 (박스/벽/캐릭터 검사)
	bool IsLocationFree(const FVector& Center) const;

	class ACSGridBoard* FindNearestBoard() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "GridBox")
	TObjectPtr<class UStaticMeshComponent> Mesh;

	// false 면 밀어도 움직이지 않는 고정 박스
	UPROPERTY(EditAnywhere, Category = "GridBox")
	bool bPushable = true;

	// 한 칸 이동에 걸리는 시간(초)
	UPROPERTY(EditAnywhere, Category = "GridBox", meta = (ClampMin = "0.01"))
	float MoveDuration = 0.15f;

	// 좌표 기준이 될 보드. 비워두면 가장 가까운 보드를 자동 탐색.
	UPROPERTY(EditInstanceOnly, Category = "GridBox")
	TObjectPtr<class ACSGridBoard> Board;

	// 보드가 없을 때 한 칸으로 사용할 거리
	UPROPERTY(EditAnywhere, Category = "GridBox")
	float FallbackCellSize = 200.0f;

	UPROPERTY(EditAnywhere, Category = "GridBox")
	FLinearColor PushableColor = FLinearColor(0.80f, 0.45f, 0.10f);

	UPROPERTY(EditAnywhere, Category = "GridBox")
	FLinearColor ImmovableColor = FLinearColor(0.15f, 0.15f, 0.18f);

protected:
	// 이 속도 미만으로 움직이는 캐릭터의 접촉은 미는 것으로 치지 않는다.
	static constexpr float MinPushSpeed = 30.0f;

	bool bMoving = false;
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveTarget = FVector::ZeroVector;
	float MoveElapsed = 0.0f;
};
