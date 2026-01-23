// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSConveyorManager.generated.h"

class USplineComponent;
class UInstancedStaticMeshComponent;
class ACSConveyorPlatform;

UCLASS()
class CHRONOSPACE_API ACSConveyorManager : public AActor
{
	GENERATED_BODY()
	
public:
	ACSConveyorManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== Accessors =====
	USplineComponent* GetSpline() const { return Spline; }
	float GetSplineLength() const { return SplineLength; }
	float GetSmoothedDistance() const { return SmoothedDistance; }
	float GetMoveSpeed() const { return MoveSpeed; }

protected:
	virtual void BeginPlay() override;

	// 에디터/블루프린트에서 재생성용
	UFUNCTION(BlueprintCallable, Category = "Conveyor")
	void RebuildConveyor();

	// =========================
	// Conveyor Logic
	// =========================

	// cm/s
	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Logic")
	float MoveSpeed = 300.f;

	// client smoothing
	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Logic")
	float InterpSpeed = 8.f;

	// replicated progress along spline
	UPROPERTY(ReplicatedUsing = OnRep_RepDistance)
	float RepDistance = 0.f;

	UFUNCTION()
	void OnRep_RepDistance();

	// =========================
	// Conveyor Shape
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Conveyor|Shape")
	USplineComponent* Spline;

	// =========================
	// Conveyor Visual
	// =========================

	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Visual")
	UStaticMesh* BeltMesh;

	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Visual")
	FVector MeshScale = FVector(1.f);

	// 0이면 Mesh 길이 자동 사용
	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Visual")
	float MeshSpacing = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Default|Conveyor|Visual")
	UInstancedStaticMeshComponent* BeltISM;

	// =========================
	// Conveyor Platforms
	// =========================

	// 숨겨진 실제 바닥 클래스
	UPROPERTY(EditAnywhere, Category = "Default|Conveyor|Platform")
	TSubclassOf<ACSConveyorPlatform> ConveyorPlatformClass;

	UPROPERTY()
	TArray<ACSConveyorPlatform*> Platforms;

	// =========================
	// Internal
	// =========================

	void BuildBeltMeshes();
	void BuildPlatforms();

private:
	float SplineLength = 0.f;

	// client-side smoothing
	float TargetDistance = 0.f;
	float SmoothedDistance = 0.f;
};
