// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/CSObjectResetComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "CSObjectResetSubsystem.generated.h"

/**
 * UCSObjectResetComponent 를 단 오브젝트들의 등록처.
 *
 * "체크포인트에서 재시작하면 오브젝트가 전부 원래 자리로" 를 붙일 자리다.
 * 지금은 ResetAll 을 부르는 곳이 없다 - 붙일 때는 부활 처리(ACSGameMode::RespawnSinglePlayer 등)에서
 * ResetAll(ECSObjectResetReason::Checkpoint) 한 줄만 부르면 된다.
 *
 * 레벨을 통째로 훑지 않고 등록된 것만 보므로 오브젝트가 늘어나도 비용이 그대로다.
 * (UCSManagedActorSubsystem 과 같은 방식)
 */
UCLASS()
class CHRONOSPACE_API UCSObjectResetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 월드에서 서브시스템을 꺼낸다. 월드가 없으면 nullptr. */
	static UCSObjectResetSubsystem* Get(const UObject* WorldContextObject);

	void Register(UCSObjectResetComponent* Component);
	void Unregister(UCSObjectResetComponent* Component);

	/**
	 * 등록된 오브젝트를 전부 홈으로 되돌린다. 서버에서만 의미가 있다.
	 * @return 실제로 되돌린 개수
	 */
	UFUNCTION(BlueprintCallable, Category = "ObjectReset")
	int32 ResetAll(ECSObjectResetReason Reason = ECSObjectResetReason::Checkpoint);

	/** 등록된 컴포넌트 수. 디버그용. */
	UFUNCTION(BlueprintPure, Category = "ObjectReset")
	int32 GetRegisteredCount() const { return RegisteredComponents.Num(); }

private:
	/**
	 * 약참조로 들고 있다가 쓸 때 걸러낸다.
	 * 컴포넌트가 EndPlay 없이 사라지는 경로가 있어도 죽은 참조로 크래시나지 않는다.
	 */
	TArray<TWeakObjectPtr<UCSObjectResetComponent>> RegisteredComponents;
};
