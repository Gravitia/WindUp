// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSMeasuringTape.generated.h"

UCLASS()
class CHRONOSPACE_API ACSMeasuringTape : public AActor
{
	GENERATED_BODY()
	
public:	
	ACSMeasuringTape();
	
	/* Meshes */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Mesh")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Mesh")
	UStaticMeshComponent* EyesMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Mesh")
	UStaticMeshComponent* NoseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Mesh")
	UStaticMeshComponent* ButtonMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Mesh")
	UStaticMeshComponent* RulerMesh; 
	
	/* Trigger */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	class UBoxComponent* Trigger;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	/* Sound */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sound")
	UAudioComponent* RulerAudioComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|MeasuringTape|Sound")
	USoundBase* ExtendSound;   // 늘어날 때

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|MeasuringTape|Sound")
	USoundBase* RetractSound;  // 줄어들 때

	/* */

	UPROPERTY(ReplicatedUsing = OnRep_TargetScale)
	float TargetScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "CSEditable|MeasuringTape|Ruler")
	float LerpSpeed = 5.0f;  // 부드럽게 늘어나는 속도

	UPROPERTY(EditAnywhere, Category = "CSEditable|MeasuringTape|Ruler")
	float TargetRulerScale = 5.0f;  // Ruler Scale

	// 연출 속도
	UPROPERTY(EditAnywhere, Category = "CSEditable|MeasuringTape|Face")
	float FaceLerpSpeed = 8.f;

	// 상태
	UPROPERTY(ReplicatedUsing = OnRep_FaceReact)
	bool bFaceReacting = false;

	float CurrentScaleInternal = 1.0f;
	bool bIsExtending = false;
	bool bIsRetracting = false;

	UFUNCTION()
	void OnRep_TargetScale();

	void SetRulerScale(float NewScale);

	UFUNCTION()
	void OnRep_FaceReact();

	/*Face Reaction (Eyes / Nose) */
	FRotator EyesBaseRotation;
	FRotator NoseBaseRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Face")
	FRotator EyesReactRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|MeasuringTape|Face")
	FRotator NoseReactRotation;

	UPROPERTY(Replicated)
	FRotator EyesTargetRotation;
	UPROPERTY(Replicated)
	FRotator NoseTargetRotation;
};
