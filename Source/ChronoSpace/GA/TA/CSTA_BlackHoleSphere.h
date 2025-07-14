// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "CSTA_BlackHoleSphere.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlackHoleEndedDelegate);

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSTA_BlackHoleSphere : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()
	
public:
	ACSTA_BlackHoleSphere();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void ConfirmTargetingAndContinue() override;

	// 블랙홀 오프셋 설정 함수 추가
	UFUNCTION(BlueprintCallable)
	void SetBlackHoleOffset(const FVector& InOffset) { BlackHoleOffset = InOffset; }

	FBlackHoleEndedDelegate OnComplete;

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);
	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION()
	void OnEventHorizonBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> GravitySphereTrigger;
	UPROPERTY(VisibleAnywhere, Category = "Sphere", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> EventHorizonSphereTrigger;
	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> StaticMeshComp;
	UPROPERTY()
	TMap<FName, TObjectPtr<ACharacter> > CharactersInSphereTrigger;
	UPROPERTY(EditAnywhere, Category = "Sphere")
	float GravityInfluenceRange = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Sphere")
	float EventHorizonRadius = 50.0f;	// 구 사이즈가 100 * 100 * 100
	bool bShowDebug = true;

private:
	// 플레이어 따라다니기용 변수들
	FVector BlackHoleOffset = FVector(600.0f, 0.0f, 200.0f);
	UPROPERTY()
	TWeakObjectPtr<AActor> PlayerActor;

	// 플레이어 따라다니기 함수들
	void UpdateBlackHolePosition();
	FVector CalculateTargetPosition() const;
};
