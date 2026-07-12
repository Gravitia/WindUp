// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Common/CSAbilityTypes.h"
#include "Interface/CSReactorTarget.h"
#include "CSAbilityReactorBase.generated.h"

class USphereComponent;

// 서버 전용: 리액터 활성 상태가 바뀔 때 방송. ACSReactorGroup 등이 구독한다.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCSReactorActivationChanged, class ACSAbilityReactorBase* /*Reactor*/, bool /*bActivated*/);

/**
 * 캐릭터 능력(Blackhole / GravityCore)이 닿았을 때 반응하는 오브젝트의 베이스 클래스.
 *
 * 감지 / 활성화 상태머신 / 1회·반복 / 네트워크 리플리케이션을 전부 여기서 처리한다.
 * 기획자는 이 클래스를 상속한 BP에서
 *   - RespondsToAbilities / Mode 만 디테일 패널에서 세팅하고
 *   - OnActivated / OnDeactivated 이벤트에 "효과"(A~E)만 구현
 * 하면 된다. 오버랩→캐스팅→SET→Branch 배선을 매번 다시 그릴 필요가 없다.
 *
 * [타겟 트리거] 다른 액터(문/다리/다른 리액터)를 트리거하려면 별도 액터 없이
 * 리액터 하나를 "대표"로 정해 TargetActors 에 타겟을, LinkedReactors 에 함께
 * 조건을 이룰 리액터들을 지정한다. TriggerLogic(All/Any)에 따라 조합이 완성되면
 * 타겟들의 ICSReactorTarget::OnReactorTriggerChanged 가 호출된다.
 *
 * 리액터 자신도 ICSReactorTarget 이므로 타겟이 될 수 있다.
 * 외부 트리거와 능력 오버랩 중 하나라도 조건을 채우면 활성된다(OR).
 * → "조합 완성 → 다음 리액터 활성 → 또 다른 조합의 입력" 식의 체인 구성 가능.
 */
UCLASS(Abstract, Blueprintable)
class CHRONOSPACE_API ACSAbilityReactorBase : public AActor, public ICSReactorTarget
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

	// 서버 전용: 활성 상태 변경 방송. ACSReactorGroup 이 BeginPlay 에서 구독한다.
	FOnCSReactorActivationChanged OnReactorActivationChanged;

	// ICSReactorTarget: 이 리액터가 다른 리액터의 타겟일 때 외부에서 켜고 끈다. (서버에서만 반영)
	virtual void OnReactorTriggerChanged_Implementation(bool bActive) override;

	// 조합(나 + LinkedReactors)이 완성된 상태인지.
	UFUNCTION(BlueprintPure, Category = "Reactor|Trigger")
	bool IsTriggerActive() const { return bTriggerActive; }

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

	// 서버 전용: 트리거 오버랩 수/모드로부터 이 리액터의 조건을 재계산해 적용.
	void RecomputeLocalCondition();

	// 서버 전용: 오버랩 조건 OR 외부 트리거 조건으로 최종 활성 여부를 적용.
	void ApplyActivationFromConditions();
	void SetActivated(bool bNewActivated);

	// ==== [타겟 트리거] 조합 판정과 타겟 통지 ====
	void HandleLinkedReactorChanged(ACSAbilityReactorBase* Reactor, bool bActivated);

	// 서버 전용: 나 + LinkedReactors 상태로 조합 완성 여부를 재계산.
	void EvaluateTrigger(bool bIgnoreLatch = false);
	void SetTriggerActive(bool bNewActive);

	// 서버/클라 양쪽: 타겟들에게 통지.
	void HandleTriggerStateChanged();
	void NotifyTargets(bool bActive);

	UFUNCTION()
	void OnRep_TriggerActive();

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

	// 감지 트리거 반경.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor")
	float TriggerRadius = 100.0f;

	// ==== [타겟 트리거] 세팅. 타겟을 가진 "대표" 리액터 한쪽에만 지정한다 ====
	// 나와 함께 조합 조건을 이룰 리액터들. 비워두면 나 혼자 조건.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reactor|Trigger")
	TArray<TObjectPtr<ACSAbilityReactorBase>> LinkedReactors;

	// All = 나+링크 전부 활성(AND) / Any = 하나라도 활성(OR).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor|Trigger")
	ECSReactorGroupLogic TriggerLogic = ECSReactorGroupLogic::All;

	// true 면 조합이 한번 완성되면 유지. ResetReactor 로 재무장.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor|Trigger")
	bool bTriggerLatch = false;

	// 조합 완성/해제 시 통지할 타겟들. ICSReactorTarget 구현 필요 (문/다리/다른 리액터 등).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reactor|Trigger")
	TArray<TObjectPtr<AActor>> TargetActors;

	// 켜면 오버랩/활성 변화를 화면·로그에 출력해 감지 문제를 진단할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactor|Debug")
	bool bReactorDebug = false;

protected:
	// 현재 트리거를 겹치고 있는 관심 능력 소스(컴포넌트) 수. 서버 전용.
	int32 OverlapRefCount = 0;

	// 모드(Latch/Toggle) 적용 후의 이 리액터 조건. 서버 전용.
	bool bLocalCondition = false;

	// 외부 트리거(그룹 등)가 이 리액터를 켜둔 상태인지. 서버 전용. Latch 모드면 한번 켜지면 유지.
	bool bExternalCondition = false;

	// 최종 활성 상태 (리플리케이트).
	UPROPERTY(ReplicatedUsing = OnRep_IsActivated)
	bool bIsActivated = false;

	// 조합(나 + LinkedReactors) 완성 상태 (리플리케이트).
	UPROPERTY(ReplicatedUsing = OnRep_TriggerActive)
	bool bTriggerActive = false;
};
