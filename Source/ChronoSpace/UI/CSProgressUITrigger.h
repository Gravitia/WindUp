// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSProgressUITrigger.generated.h"

class UBoxComponent;
class UCSProgressUIWidget;

UCLASS()
class CHRONOSPACE_API ACSProgressUITrigger : public AActor
{
	GENERATED_BODY()

public:
	ACSProgressUITrigger();

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

protected:
	/* ================= Components ================= */

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;

	/* ================= Progress UI ================= */

	/** 표시할 Progress UI 위젯 클래스 (BP) */
	UPROPERTY(EditAnywhere, Category = "Progress UI")
	TSubclassOf<UCSProgressUIWidget> ProgressUIWidgetClass;

	/** 전달할 텍스트 ID */
	UPROPERTY(EditAnywhere, Category = "Progress UI")
	FName ProgressTextId;

	/** 표시 유지 시간 */
	UPROPERTY(EditAnywhere, Category = "Progress UI")
	float DisplayDuration = 3.0f;

	/* ================= State ================= */

	bool bTriggered = false;

};
