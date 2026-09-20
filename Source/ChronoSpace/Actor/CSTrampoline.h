// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSTrampoline.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ACharacter;

/**
 * 밟으면 튕겨 올리는 트램펄린.
 *
 * 기획자는 BP 를 상속해 메시만 바꿔 끼우고 디테일 패널에서 세기만 조절하면 된다.
 * 연출(스쿼시 애니메이션, 사운드, VFX)은 OnLaunched 이벤트에 구현한다.
 *
 * [방향] 튕기는 방향은 이 액터의 위쪽(Up)이다. 액터를 기울여 놓으면 비스듬히 날아간다.
 * 월드 Z 가 아니라 액터 기준이라 중력 반전으로 천장에 붙은 상황에서도 그대로 쓸 수 있다.
 *
 * [멀티] 발사는 서버와 그 캐릭터를 조종하는 머신에서 각각 돈다. 양쪽이 같은 식으로
 * 계산하므로 클라는 즉시 반응하고 서버가 정답을 보정한다. 다른 클라의 시뮬레이션
 * 프록시는 건드리지 않는다. 결과가 복제되어 따라온다.
 */
UCLASS()
class CHRONOSPACE_API ACSTrampoline : public AActor
{
	GENERATED_BODY()

public:
	ACSTrampoline();

	/** 튕기는 방향(월드). 이 액터의 위쪽이다. */
	UFUNCTION(BlueprintPure, Category = "Trampoline")
	FVector GetLaunchDirection() const;

protected:
	virtual void BeginPlay() override;

	// =========================
	// Components
	// =========================
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 이 볼륨에 들어오면 튕긴다. 메시보다 살짝 위에 얹어 두는 것을 권한다. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> BounceTrigger;

	// =========================
	// 세기
	// =========================
	/**
	 * 튕겨 올리는 속도 (cm/s). 도달 높이는 대략 속도의 제곱을 중력의 두 배로 나눈 값이다.
	 *
	 * 이 프로젝트는 캐릭터 GravityScale 이 5 라 중력이 4900 이다. 기본 중력 980 을
	 * 기준으로 감을 잡으면 다섯 배 낮게 뜨니 주의할 것. 실제 높이는 이렇다.
	 *
	 *   1800 (캐릭터 자체 점프와 같은 값)  ->  약 330
	 *   3000                               ->  약 918
	 *   4000                               ->  약 1633
	 *   6000                               ->  약 3673
	 *
	 * 기본값은 자체 점프의 약 2.8배 높이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Trampoline", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 3000.f;

	/**
	 * true  : 튕기는 방향 속도를 LaunchSpeed 로 "덮어쓴다". 얼마나 세게 떨어졌든 높이가 같다.
	 * false : 기존 속도에 "더한다". 높이 떨어질수록 더 높이 튄다.
	 *
	 * 정해진 높이의 발판을 넘는 퍼즐이면 true, 점점 높이 튀는 놀이터 느낌이면 false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Trampoline")
	bool bOverrideLaunchSpeed = true;

	/**
	 * 튕길 때 남겨 둘 수평 속도의 비율. 1 이면 달려온 기세를 그대로 유지하고,
	 * 0 이면 제자리에서 수직으로만 올라간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Trampoline", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float HorizontalVelocityScale = 1.f;

	/**
	 * 같은 캐릭터를 다시 튕기기까지의 최소 간격(초).
	 * 트리거를 벗어났다 바로 다시 들어오며 두 번 발사되는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, Category = "CSEditable|Trampoline", meta = (ClampMin = "0.0"))
	float RetriggerCooldown = 0.2f;

	/**
	 * 튕기려면 최소한 이 속도로 내려와 부딪혀야 한다 (cm/s).
	 *
	 * 걸어서 지나가거나 그 위에 가만히 서 있는 것으로는 튕기지 않는다. 뛰어올랐다가
	 * 떨어지거나 높은 데서 낙하해 "찍었을" 때만 작동한다.
	 *
	 * 캐릭터 자체 점프(JumpZVelocity 1800)로 제자리 점프하면 약 1800 으로 내려오므로,
	 * 기본값 800 이면 점프 착지는 잡히고 작은 턱에서 내려오는 정도는 걸러진다.
	 * 0 으로 두면 닿기만 해도 튕긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Trampoline", meta = (ClampMin = "0.0"))
	float MinLandingSpeed = 800.f;

	/** true 면 플레이어만 튕긴다. false 면 AI 캐릭터도 튕긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Trampoline")
	bool bPlayersOnly = true;

	// =========================
	// 연출 훅
	// =========================
	/**
	 * 실제로 튕겨 냈을 때 호출된다. 스쿼시 애니메이션, 사운드, VFX 를 여기 붙인다.
	 *
	 * 주의: 서버와 그 캐릭터를 조종하는 클라에서 각각 한 번씩 불린다. 다른 클라에서는
	 * 불리지 않는다. 모두에게 보여야 하는 연출이면 Multicast 로 따로 쏘아야 한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Trampoline", meta = (DisplayName = "On Launched"))
	void OnLaunched(ACharacter* LaunchedCharacter, FVector LaunchVelocity);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnBounceBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBounceEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/** 이 캐릭터를 지금 튕겨도 되는가. 필터, 쿨다운, 내려오는 중인지를 본다. */
	bool CanLaunch(const ACharacter* Character) const;

	/** 실제로 튕긴다. ImpactVelocity 는 부딪히던 그 순간의 속도다. */
	void LaunchNow(ACharacter* Character, const FVector& ImpactVelocity);

	/**
	 * 다음 틱에 튕겨 줄 캐릭터들.
	 *
	 * 판정과 발사를 서로 다른 시점에 나눠 하는 이유가 각각 있다.
	 *
	 * 판정은 오버랩이 들어오는 "그 순간" 해야 한다. 캐릭터가 빠르게 떨어지면 트리거에
	 * 들어온 프레임에 바로 착지해 속도가 0이 된다. 다음 틱에 속도를 보면 이미 늦어서
	 * 부딪힌 세기를 알 수 없다.
	 *
	 * 발사는 반대로 오버랩 콜백에서 하면 안 되고 틱에서 해야 한다.
	 * UCharacterMovementComponent::PerformMovement 의 순서가
	 *   HandlePendingLaunch() -> ClearAccumulatedForces() -> StartNewPhysics()
	 * 인데 오버랩은 맨 뒤 StartNewPhysics 안에서 발생한다. 거기서 예약한 발사는
	 * 이미 지나간 HandlePendingLaunch 를 놓치고 같은 프레임의 ClearAccumulatedForces 에
	 * 지워진다. 엔진 소스에도 같은 함정이 주석으로 적혀 있다.
	 */
	TMap<TWeakObjectPtr<ACharacter>, FVector> PendingLaunch;

	/** 캐릭터별 마지막 발사 시각. 쿨다운 판정용. */
	TMap<TWeakObjectPtr<ACharacter>, float> LastLaunchTime;
};
