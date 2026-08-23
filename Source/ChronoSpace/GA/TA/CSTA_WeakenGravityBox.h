// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/TA/CSTA_BoxTrigger.h"
#include "CSTA_WeakenGravityBox.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSTA_WeakenGravityBox : public ACSTA_BoxTrigger
{
	GENERATED_BODY()

public:
	ACSTA_WeakenGravityBox();

	virtual void BeginPlay() override;

	virtual void StartTargeting(UGameplayAbility* Ability) override;

	virtual void ConfirmTargetingAndContinue() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FORCEINLINE void SetGravityCoef(float InGravityCoef) { GravityCoef = InGravityCoef; }

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastSaturationSetting(float InGravityCoef);

	void SaturationSetting();
	void HandleSaturationSetting(float InGravityCoef);

	// 서버가 SetGravityCoef 로 넣은 값(예: 0.5)이 클라 복제본에도 와야 한다. 복제 안 하면 클라는 기본 0.1 로
	// GravityScale 을 곱해 서버와 5배 차이 -> 매 프레임 러버밴딩. (오버랩은 서버/클라 양쪽에서 각자 적용하는 구조)
	UPROPERTY(EditAnywhere, Replicated, Category = "GravityCoef")
	float GravityCoef = 0.1f;
};
