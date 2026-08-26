// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "CSObjectResetComponent.generated.h"

/**
 * 오브젝트를 원래 자리로 되돌리게 만든 이유.
 * 되돌리는 계기는 앞으로 늘어난다 - 연출을 붙이는 쪽에서 구분할 수 있게 이유를 같이 넘긴다.
 */
UENUM(BlueprintType)
enum class ECSObjectResetReason : uint8
{
	/** 컴포넌트가 그린 볼륨을 벗어났다 */
	OutOfBounds     UMETA(DisplayName = "Out Of Bounds"),

	/** 체크포인트에서 재시작했다 (아직 부르는 곳 없음) */
	Checkpoint      UMETA(DisplayName = "Checkpoint Restart"),

	/** 코드나 블루프린트에서 직접 불렀다 */
	Manual          UMETA(DisplayName = "Manual")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCSOnObjectReset, ECSObjectResetReason, Reason);

/**
 * 오브젝트가 정해진 볼륨을 벗어나면 원래 자리로 되돌린다.
 *
 * UCSKillZoneResetComponent 와 목적은 같지만 계기가 다르다.
 * 그쪽은 킬존에 닿는 것을 신호로 쓰는데, 그러면 킬존에 닿았을 때 다른 일을 해야 하는
 * 오브젝트(프레스 등)가 되돌아가 버려서 두 기능이 서로 잡아먹는다.
 * 이 컴포넌트는 킬존을 보지 않고 자기 볼륨만 보므로 둘이 겹치지 않는다.
 *
 * 볼륨은 이 컴포넌트 자신이다. 블루프린트에서 기즈모로 크기와 위치를 잡으면 된다.
 * 범위 판정은 BeginPlay 시점의 볼륨 위치를 기준으로 한다 - 오브젝트가 굴러가도
 * 범위는 제자리에 있어야 하기 때문이다.
 *
 * 되돌리기는 서버에서만 일어나고, 위치는 복제로 전파된다.
 * 연출용 이벤트는 ResetCount 의 OnRep 을 타고 클라이언트에도 도착한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CHRONOSPACE_API UCSObjectResetComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UCSObjectResetComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 오브젝트를 홈으로 되돌린다. 서버에서만 동작한다.
	 * 체크포인트 재시작도 결국 여기로 들어온다.
	 * @return 실제로 되돌렸으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "ObjectReset")
	bool ResetToHome(ECSObjectResetReason Reason = ECSObjectResetReason::Manual);

	/** 지금 자리를 새 홈으로 삼는다. 단계별로 진행되는 퍼즐에서 중간 지점을 잡을 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "ObjectReset")
	void SetHomeToCurrentTransform();

	UFUNCTION(BlueprintPure, Category = "ObjectReset")
	FTransform GetHomeTransform() const { return HomeTransform; }

	UFUNCTION(BlueprintPure, Category = "ObjectReset")
	bool IsOwnerInsideBounds() const;

	/** 되돌린 직후. 서버와 클라이언트 양쪽에서 불린다 - 연출은 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "ObjectReset")
	FCSOnObjectReset OnObjectReset;

protected:
	/** 끄면 볼륨 검사를 하지 않는다. 체크포인트 리셋만 받고 싶을 때. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|ObjectReset")
	bool bResetWhenOutOfBounds = true;

	/** 범위 검사 주기(초). 매 프레임 볼 필요가 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|ObjectReset",
		meta = (EditCondition = "bResetWhenOutOfBounds", ClampMin = "0.02"))
	float BoundsCheckInterval = 0.2f;

	/** 되돌릴 때 홈 위치에 더할 여유. 바닥에 박힌 채 나타나는 걸 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|ObjectReset")
	FVector ResetOffset = FVector(0.f, 0.f, 50.f);

	/** 되돌릴 때 회전도 원래대로 돌릴지. 끄면 위치만 옮긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|ObjectReset")
	bool bRestoreRotation = true;

	UFUNCTION()
	void OnRep_ResetCount();

private:
	void CheckBounds();

	/** 오브젝트가 돌아갈 자리 */
	FTransform HomeTransform;

	/** BeginPlay 시점의 볼륨(월드 기준). 오브젝트가 움직여도 이 범위는 고정이다. */
	FVector BoundsOrigin = FVector::ZeroVector;
	FQuat BoundsRotation = FQuat::Identity;
	FVector BoundsExtent = FVector::ZeroVector;

	/**
	 * 연출을 클라이언트까지 보내기 위한 카운터.
	 * 서버 이벤트는 클라에 안 오므로 값의 변화로 전파한다 (UCSMeshPulledByBlackhole 과 같은 방식).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ResetCount)
	int32 ResetCount = 0;

	UPROPERTY(Replicated)
	ECSObjectResetReason LastResetReason = ECSObjectResetReason::Manual;

	FTimerHandle BoundsCheckTimer;
};
