// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSConveyorManager.generated.h"

class UInstancedStaticMeshComponent;
class ACSConveyorPlatform;

UCLASS()
class CHRONOSPACE_API ACSConveyorManager : public AActor
{
	GENERATED_BODY()
	
public:
	ACSConveyorManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	// ===== Accessors =====
	float GetSmoothedProgress() const { return SmoothedProgress; }
	float GetTotalLength() const { return TotalLength; }
	float GetConveyorSpacing() const { return ConveyorSpacing; }

protected:
	// =========================
	// Conveyor Logic
	// =========================
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Logic")
	float MoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Logic")
	float InterpSpeed = 8.f;

	UPROPERTY(ReplicatedUsing = OnRep_RepProgress)
	float RepProgress = 0.f;

	UFUNCTION()
	void OnRep_RepProgress();

	// =========================
	// Conveyor Layout
	// =========================
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Layout")
	float ConveyorSpacing = 200.f;

	// =========================
	// Conveyor Platforms
	// =========================
	// 레벨에서 매니저마다 직접 지정
	UPROPERTY(EditInstanceOnly, Category = "CSEditable|Conveyor|Platform", meta = (DisplayName = "Assigned Platforms"))
	TArray<TObjectPtr<ACSConveyorPlatform>> AssignedPlatforms;

	// 런타임용(정리/중복 제거 후 사용하는 배열)
	UPROPERTY()
	TArray<TObjectPtr<ACSConveyorPlatform>> Platforms;

	// =========================
	// Conveyor Visual (선택)
	// =========================
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Visual")
	UStaticMesh* ConveyorMesh;

	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Visual")
	UMaterialInterface* ConveyorMaterial;

	UPROPERTY(VisibleAnywhere, Category = "Default|Conveyor|Visual")
	UInstancedStaticMeshComponent* ConveyorISM;

	// =========================
	// Build
	// =========================
	UFUNCTION(BlueprintCallable, Category = "Conveyor")
	void InitializePlatformsFromAssigned();

	UFUNCTION(BlueprintCallable, Category = "Conveyor")
	void BuildVisual();

private:
	float TotalLength = 0.f;

	float TargetProgress = 0.f;
	float SmoothedProgress = 0.f;

};
