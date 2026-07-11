// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/CSMimicCharacter.h"
#include "CSDecoyCharacter.generated.h"

/**
 * 미끼 인형 - 플레이어와 닮은 태엽 인형.
 * 플레이어가 주변(WindUpRange)에 WindUpRequiredTime 동안 머물면 태엽이 감겨서
 * 전방으로 WalkDuration 동안 걸어간 뒤 멈춘다. 멈춘 후 다시 감을 수 있다.
 * ACSMimicCharacter 상속으로 죽지 않고, 컨트롤러 없이 서버에서 움직인다.
 */
UCLASS()
class CHRONOSPACE_API ACSDecoyCharacter : public ACSMimicCharacter
{
	GENERATED_BODY()

public:
	ACSDecoyCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnWindUpTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWindUpTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void StartWalking();
	void StopWalking();

	// BP 연출 훅 (서버에서 호출됨 - 필요 시 BP에서 VFX/사운드 연결)
	UFUNCTION(BlueprintImplementableEvent, Category = "Decoy")
	void OnWindUpProgress(float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Decoy")
	void OnWindUpComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "Decoy")
	void OnWalkFinished();

protected:
	// 태엽이 감기는 데 필요한 근접 유지 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy")
	float WindUpRequiredTime = 1.0f;

	// 감긴 후 걸어가는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy")
	float WalkDuration = 10.0f;

	// 태엽 감기 판정 반경 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy")
	float WindUpRange = 200.0f;

	// 근접 판정용 스피어
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoy")
	TObjectPtr<class USphereComponent> WindUpTrigger;

private:
	// 서버 전용 상태
	TSet<TWeakObjectPtr<class ACSCharacterPlayer>> NearbyPlayers;

	float WindUpElapsed = 0.0f;
	float WalkElapsed = 0.0f;
	bool bWalking = false;
	FVector WalkDirection = FVector::ForwardVector;
};
