// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CSReactorTarget.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCSReactorTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 리액터 그룹(ACSReactorGroup)의 타겟이 되는 액터가 구현하는 인터페이스.
 *
 * 문 / 다리 / 발판 등 "리액터 조합이 완성되면 반응할" 액터에 이 인터페이스를 붙이고
 * OnReactorTriggerChanged 만 구현하면 된다. (BP 에서는 Class Settings > Interfaces 에 추가)
 * 그룹이 서버/클라 양쪽에서 호출해주므로 연출은 로컬에서 바로 실행해도 된다.
 *
 * ACSAbilityReactorBase 도 이 인터페이스를 구현하므로 리액터 자신이 그룹의 타겟이 될 수 있다.
 * (그룹 완성 → 다음 리액터 활성 → 또 다른 그룹의 입력, 식의 체인)
 */
class CHRONOSPACE_API ICSReactorTarget
{
	GENERATED_BODY()

public:
	// 그룹 활성 상태가 바뀔 때 호출된다. bActive = true 면 조합 완성.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Reactor")
	void OnReactorTriggerChanged(bool bActive);
};
