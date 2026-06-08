#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSTransformMover.generated.h"

class UCurveFloat;
class USceneComponent;

USTRUCT(BlueprintType)
struct FCSTransformMovementPreset
{
	GENERATED_BODY()

	// Applied as an offset from the moving component's initial transform when enabled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bRelativeToInitial = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (FormerlySerializedAs = "TargetRelativeTransform"))
	FTransform TargetTransform = FTransform::Identity;

	// Expected to map normalized time 0..1 to movement alpha.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TObjectPtr<UCurveFloat> Curve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float Duration = 1.0f;
};

USTRUCT()
struct FCSTransformMovementState
{
	GENERATED_BODY()

	UPROPERTY()
	FName MovementKey = NAME_None;

	UPROPERTY()
	FTransform StartTransform = FTransform::Identity;

	UPROPERTY()
	FTransform TargetTransform = FTransform::Identity;

	UPROPERTY()
	float ServerStartTime = 0.0f;

	UPROPERTY()
	float Duration = 1.0f;

	UPROPERTY()
	bool bActive = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCSTransformMovementEvent, FName, MovementKey);

UCLASS(Blueprintable)
class CHRONOSPACE_API ACSTransformMover : public AActor
{
	GENERATED_BODY()

public:
	ACSTransformMover();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Transform Mover")
	bool PlayMovement(FName MovementKey);

	UFUNCTION(BlueprintCallable, Category = "Transform Mover")
	bool ReturnToInitial();

	UFUNCTION(BlueprintCallable, Category = "Transform Mover")
	void StopMovement(bool bSnapToTarget = false);

	UFUNCTION(BlueprintPure, Category = "Transform Mover")
	bool IsMoving() const { return MovementState.bActive; }

	UFUNCTION(BlueprintPure, Category = "Transform Mover")
	FName GetActiveMovementKey() const { return MovementState.MovementKey; }

	UPROPERTY(BlueprintAssignable, Category = "Transform Mover|Events")
	FCSTransformMovementEvent OnMovementStarted;

	UPROPERTY(BlueprintAssignable, Category = "Transform Mover|Events")
	FCSTransformMovementEvent OnMovementFinished;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transform Mover")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transform Mover")
	TObjectPtr<USceneComponent> MovingRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Mover")
	TMap<FName, FCSTransformMovementPreset> MovementPresets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Mover|Return", meta = (ClampMin = "0.01"))
	float ReturnDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform Mover|Return")
	TObjectPtr<UCurveFloat> ReturnCurve = nullptr;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartMovement(
		FName MovementKey,
		const FTransform& StartTransform,
		const FTransform& TargetTransform,
		float Duration);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishMovement(FName MovementKey, const FTransform& TargetTransform);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopMovement(const FTransform& TargetTransform, bool bSnapToTarget);

	bool StartMovement(
		FName MovementKey,
		const FTransform& TargetTransform,
		float Duration);
	void ApplyMovement();
	void FinishMovement();
	float GetSynchronizedWorldTime() const;
	float EvaluateAlpha(float NormalizedTime) const;
	FTransform MakeTargetTransform(const FCSTransformMovementPreset& Preset) const;

	FCSTransformMovementState MovementState;

	FTransform InitialMovingRootTransform = FTransform::Identity;
};
