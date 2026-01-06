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
	// Sets default values for this actor's properties
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

protected:
	/* ================= Components ================= */

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;

	/* ================= Progress UI ================= */

	/** 표시할 Progress UI 위젯 클래스 (BP) */
	UPROPERTY(EditAnywhere, Category = "Default")
	TSubclassOf<UCSProgressUIWidget> ProgressUIWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Default")
	bool bProgressText;

	/** 전달할 텍스트 ID */
	UPROPERTY(EditAnywhere, Category = "Default")
	FName ProgressText;

	/** 표시 유지 시간 */
	UPROPERTY(EditAnywhere, Category = "Default")
	float DisplayDuration = 3.0f;

	/* ================= State ================= */

	bool bTriggered = false;

};
