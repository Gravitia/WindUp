// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Common/CSAbilityTypes.h"
#include "CSReactorGroup.generated.h"

class ACSAbilityReactorBase;

/**
 * [사용 중단 예정] 리액터 조합/타겟 통지가 ACSAbilityReactorBase 에 내장되면서
 * (LinkedReactors / TriggerLogic / TargetActors) 이 클래스는 더 이상 필요 없다.
 * 레벨의 BP_CSReactorGroup 정리가 끝나면 삭제할 것.
 *
 * 리액터 조합기. "리액터는 감지만, 조합과 타겟 통지는 그룹이" 하는 구조의 중심.
 *
 * 레벨에 하나 놓고
 *   - Reactors     : 조합할 리액터들 (예: 중력코어용 1개 + 블랙홀용 1개)
 *   - Logic        : All(AND) / Any(OR)
 *   - TargetActors : 조합 완성 시 반응할 액터들 (ICSReactorTarget 구현 필요)
 * 만 지정하면 된다.
 *
 * 판정은 서버에서 하고 결과(bGroupActive)는 리플리케이트되며,
 * 서버/클라 양쪽에서 타겟의 OnReactorTriggerChanged 와 그룹 BP 이벤트가 호출된다.
 */
UCLASS()
class CHRONOSPACE_API ACSReactorGroup : public AActor
{
	GENERATED_BODY()

public:
	ACSReactorGroup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "ReactorGroup")
	bool IsGroupActive() const { return bGroupActive; }

	// 그룹(과 원하면 소속 리액터)을 초기 상태로 재무장. 체크포인트/리셋용. (서버 권한)
	UFUNCTION(BlueprintCallable, Category = "ReactorGroup")
	void ResetGroup(bool bAlsoResetReactors = true);

protected:
	virtual void BeginPlay() override;

	// ==== 기획자 BP 훅 (그룹 자체 연출용, 서버/클라 모두 호출) ====
	UFUNCTION(BlueprintImplementableEvent, Category = "ReactorGroup", meta = (DisplayName = "On Group Activated"))
	void OnGroupActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "ReactorGroup", meta = (DisplayName = "On Group Deactivated"))
	void OnGroupDeactivated();

protected:
	void HandleReactorActivationChanged(ACSAbilityReactorBase* Reactor, bool bActivated);

	// 서버 전용: 소속 리액터 상태로부터 그룹 활성 여부를 재계산.
	void EvaluateGroup(bool bIgnoreLatch = false);
	void SetGroupActive(bool bNewActive);

	// 서버/클라 양쪽: BP 이벤트 + 타겟 통지 실행.
	void HandleGroupChanged();
	void NotifyTargets(bool bActive);

	UFUNCTION()
	void OnRep_GroupActive();

protected:
	// 조합할 리액터들.
	UPROPERTY(EditInstanceOnly, Category = "ReactorGroup")
	TArray<TObjectPtr<ACSAbilityReactorBase>> Reactors;

	// All = 전부 활성일 때(AND) / Any = 하나라도 활성일 때(OR).
	UPROPERTY(EditAnywhere, Category = "ReactorGroup")
	ECSReactorGroupLogic Logic = ECSReactorGroupLogic::All;

	// true 면 한번 조합이 완성된 뒤에는 리액터가 꺼져도 그룹 활성을 유지. ResetGroup 으로 재무장.
	UPROPERTY(EditAnywhere, Category = "ReactorGroup")
	bool bLatch = false;

	// 조합 완성/해제 시 반응할 타겟들. ICSReactorTarget 을 구현해야 한다.
	UPROPERTY(EditInstanceOnly, Category = "ReactorGroup")
	TArray<TObjectPtr<AActor>> TargetActors;

	// 상태 변화를 로그로 출력.
	UPROPERTY(EditAnywhere, Category = "ReactorGroup|Debug")
	bool bGroupDebug = false;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_GroupActive)
	bool bGroupActive = false;
};
