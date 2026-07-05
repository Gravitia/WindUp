// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Common/CSAbilityTypes.h"
#include "CSAbilityReactorBase.generated.h"

class USphereComponent;

/**
 * 캐릭터 능력(Blackhole / GravityCore)이 닿았을 때 반응하는 오브젝트의 베이스 클래스.
 *
 * 감지 / 활성화 상태머신 / 1회·반복 / 짝(Pair) / 네트워크 리플리케이션을 전부 여기서 처리한다.
 * 기획자는 이 클래스를 상속한 BP에서
 *   - RespondsToAbilities / Mode / PairLogic 만 디테일 패널에서 세팅하고
 *   - OnActivated / OnDeactivated 이벤트에 "효과"(A~E)만 구현
 * 하면 된다. 오버랩→캐스팅→SET→Branch 배선을 매번 다시 그릴 필요가 없다.
 */
UCLASS(Abstract, Blueprintable)
class CHRONOSPACE_API ACSAbilityReactorBase : public AActor
{
	GENERATED_BODY()

public:
	ACSAbilityReactorBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 활성 상태.
	UFUNCTION(BlueprintPure, Category = "Reactor")
	bool IsActivated() const { return bIsActivated; }

	// 래치(1회) 리액터를 다시 비활성으로 복귀시켜 재무장. 체크포인트/리셋용. (서버 권한)
	UFUNCTION(BlueprintCallable, Category = "Reactor")
	void ResetReactor();

protected:
	virtual void BeginPlay() override;

	// ==== 기획자가 BP에서 구현하는 "효과" 훅 (여기만 오브젝트마다 다름) ====
	UFUNCTION(BlueprintImplementableEvent, Category = "Reactor", meta = (DisplayName = "On Activated"))
	void OnActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Reactor", meta = (DisplayName = "On Deactivated"))
	void OnDeactivated();

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_IsActivated();

	// OtherActor 가 ICSAbilitySource 이고, RespondsToAbilities 에 포함된 능력인지.
	bool IsAbilitySourceOfInterest(const AActor* OtherActor) const;

	// 서버 전용: 트리거 오버랩 수/모드로부터 이 리액터의 로컬 조건을 재계산.
	void RecomputeLocalCondition();

	// 서버 전용: 로컬 조건 + 짝 로직으로 최종 활성 여부를 계산해 적용.
	void EvaluateActivation();
	bool ComputeDesiredActivation() const;
	void SetActivated(bool bNewActivated);

	// 서버/클라 양쪽에서 활성 상태 진입 시 실제 반응(OnActivated/OnDeactivated)을 부른다.
	void HandleActivationChanged();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reactor")
	TObjectPtr<USphereComponent> Trigger;

	// [축1] 어떤 능력에 반응할지. 디테일 패널에서 체크박스 다중 선택.
	// 아무것도 체크하지 않으면(0) 모든 능력에 반응한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor",
		meta = (Bitmask, BitmaskEnum = "/Script/ChronoSpace.ECSAbilityType"))
	int32 RespondsToAbilities = 0;

	// [축2] 1회(Latch) / 반복(Toggle).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor")
	ECSReactorMode Mode = ECSReactorMode::Toggle;

	// [축3] 단독 / 짝(AND·OR).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor|Pair")
	ECSReactorPairLogic PairLogic = ECSReactorPairLogic::None;

	// 짝이 될 리액터. 한쪽에만 지정해도 BeginPlay 에서 서로 연결된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reactor|Pair")
	TObjectPtr<ACSAbilityReactorBase> PairedReactor;

	// 감지 트리거 반경.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor")
	float TriggerRadius = 100.0f;

	// 켜면 오버랩/활성 변화를 화면·로그에 출력해 감지 문제를 진단할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor|Debug")
	bool bReactorDebug = false;

protected:
	// 현재 트리거를 겹치고 있는 관심 능력 소스(컴포넌트) 수. 서버 전용.
	int32 OverlapRefCount = 0;

	// 짝 로직 적용 전, 이 리액터 자체의 조건. 서버 전용.
	bool bLocalCondition = false;

	// 최종 활성 상태 (리플리케이트).
	UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
	bool bIsActivated = false;
};
