// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSMeshAffectedByGravityCore.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGravityCoreInteractionStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGravityCoreInteractionEnded);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSMeshAffectedByGravityCore : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSMeshAffectedByGravityCore();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 서버 전용. 이 메시에 영향을 주는 소스(그래비티 코어)를 등록/해제한다.
	 * 카운트가 복제되므로 클라도 OnRep 으로 연출(버튼 점등)을 받는다 -
	 * 예전엔 서버에서만 브로드캐스트해 호스트 화면에서만 버튼이 켜졌다.
	 */
	void AddInfluence();
	void RemoveInfluence();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInfluenced() const { return InfluenceCount > 0; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnGravityCoreInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnGravityCoreInteractionEnded OnInteractionEnded;

	UFUNCTION( BlueprintCallable )
	void SetEnable( bool bInEnable );

	UFUNCTION( BlueprintCallable )
	bool IsEnable() { return bEnable; }

protected:
	UFUNCTION()
	void OnRep_InfluenceCount();

	UPROPERTY(ReplicatedUsing = OnRep_InfluenceCount)
	int32 InfluenceCount = 0;

	// 0 <-> N 전이에서만 델리게이트를 발화하기 위한 로컬 상태
	bool bBroadcastActive = false;

	bool bEnable{ true };
};
