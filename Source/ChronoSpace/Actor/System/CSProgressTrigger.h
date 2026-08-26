// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSProgressTrigger.generated.h"

class UBoxComponent;
class UCSProgressUIWidget;

UCLASS()
class CHRONOSPACE_API ACSProgressTrigger : public AActor
{
	GENERATED_BODY()
	
public:
	ACSProgressTrigger();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	/* ================= Components ================= */

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;

	/* ================= Progress UI ================= */

	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger|UI")
	TSubclassOf<UCSProgressUIWidget> ProgressUIWidgetClass;

	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger|UI")
	bool bProgressText = true;

	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger|UI")
	FName ProgressText;

	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger|UI")
	float DisplayDuration = 3.0f;

	/* ================= Targets ================= */

	// 트리거에 닿으면 Activate 할 대상들(레벨에 배치된 액터 드래그로 넣기)
	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger")
	TArray<TObjectPtr<AActor>> ActivateTargets;

	// 트리거에 닿으면 Deactivate 할 대상들
	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger")
	TArray<TObjectPtr<AActor>> DeactivateTargets;

	// 트리거 ID (디버그/로그용)
	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger")
	FName TriggerId = NAME_None;

	// 한 번만 발동할지
	UPROPERTY(EditAnywhere, Category = "CSEditable|ProgressTrigger")
	bool bTriggerOnce = true;

	/* ================= State ================= */

	UPROPERTY(VisibleInstanceOnly, Category = "Default|Progress")
	bool bTriggered = false;

};
