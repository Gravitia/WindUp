// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSMeshPulledByBlackhole.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackholeInteractionStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackholeInteractionEnded);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSMeshPulledByBlackhole : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSMeshPulledByBlackhole();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 서버 전용. 이 메시에 영향을 주는 소스(블랙홀)를 등록/해제한다.
	 * 0 -> 1 에서 물리 상태를 저장하고 적용, 1 -> 0 에서만 원래대로 되돌린다.
	 * (블랙홀 두 개가 겹쳤을 때 먼저 나간 쪽이 중력을 켜버리던 문제를 막는다)
	 */
	void AddInfluence();
	void RemoveInfluence();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInfluenced() const { return InfluenceCount > 0; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnBlackholeInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnBlackholeInteractionEnded OnInteractionEnded;

protected:
	UFUNCTION()
	void OnRep_InfluenceCount();

	/** 서버에서만 - 원래 물리 상태를 저장하고 블랙홀 영향 상태로 바꾼다 */
	void SaveAndApplyAffectedState();
	void RestoreAffectedState();

	// 델리게이트를 복제로 전파하기 위한 카운트. 연출(버튼 점등)은 서버 이벤트가 아니라 이 값의 OnRep 으로 돈다.
	UPROPERTY(ReplicatedUsing = OnRep_InfluenceCount)
	int32 InfluenceCount = 0;

	// 마지막으로 브로드캐스트한 상태 (0 <-> N 전이에서만 발화)
	bool bBroadcastActive = false;

	// 서버 전용 저장 상태
	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> AffectedMesh;
	bool bSavedGravityEnabled = true;
	TEnumAsByte<ECollisionResponse> SavedCameraResponse = ECR_Block;
	bool bHasSavedState = false;
};
