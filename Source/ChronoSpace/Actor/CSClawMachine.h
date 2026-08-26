// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSClawMachine.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACSCharacterPlayer;

/* ─────────────────────────────────────────────
 *  State
 * ───────────────────────────────────────────── */

UENUM(BlueprintType)
enum class EClawMachineState : uint8
{
	Home         UMETA(DisplayName = "대기"),
	Diving       UMETA(DisplayName = "잡으러 내려가는 중"),
	Retracting   UMETA(DisplayName = "잡지 못해 복귀 중"),
	Lifting      UMETA(DisplayName = "잡고 들어올리는 중"),
	Translating  UMETA(DisplayName = "좌우 이동 중"),
	Dropping     UMETA(DisplayName = "목적지로 내리는 중"),
	Returning    UMETA(DisplayName = "비우고 복귀 중")
};

/**
 * 인형뽑기 트랩.
 *
 *  Trigger에 플레이어가 닿으면 위쪽의 Claw가 빠르게 내려와 잡고,
 *  Lift → Translate → Drop의 3단 모션으로 Destination까지 강제 이동시킨다.
 *  잡기 전에 플레이어가 Trigger에서 빠져나가면 Claw는 즉시 Home으로 복귀한다.
 *
 *  모션 흐름 (옆에서 본 모습):
 *
 *     home ──→ lift ─────translate─────→ drop      <- Claw 경로
 *      ↓                                  ↓
 *    dive                              destination
 *      ↓
 *   trigger (플레이어 위치)
 *
 *  네트워크: 액터 자체가 Claw라서 SetReplicateMovement(true)로 위치 자동 동기화.
 *           State / GrabbedPlayer만 Replicated. AttachToComponent는 자동 동기화.
 */
UCLASS()
class CHRONOSPACE_API ACSClawMachine : public AActor
{
	GENERATED_BODY()

public:
	ACSClawMachine();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	/* ─────────────────────────────────────────────
	 *  Components
	 * ───────────────────────────────────────────── */

	/** 액터 루트 = 움직이는 Claw. 이 컴포넌트의 World 위치 = 클로 위치 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClawMachine|Components")
	TObjectPtr<UStaticMeshComponent> Claw;

	/** 플레이어 감지 트리거. BeginPlay 시점의 World 위치/회전/스케일에 고정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClawMachine|Components")
	TObjectPtr<UBoxComponent> Trigger;

	/* ─────────────────────────────────────────────
	 *  Config
	 * ───────────────────────────────────────────── */

	/**
	 * 플레이어를 옮길 목적지. MakeEditWidget 이라 뷰포트 위젯으로 찍으면 이 값은 액터 로컬 좌표로 저장된다
	 * (엔진 규약). 런타임에는 GetDestinationWorld() 로 월드 좌표로 변환해서 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine", meta = (MakeEditWidget = true))
	FVector Destination = FVector(0.0f, 0.0f, 0.0f);

	/** 잡은 후 들어올릴 높이 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine")
	float LiftHeight = 1000.0f;

	/** 잡으러 빠르게 내려가는 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float DiveSpeed = 2000.0f;

	/** 잡지 못하고 복귀하는 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float RetractSpeed = 1500.0f;

	/** 잡은 채 들어올리는 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float LiftSpeed = 800.0f;

	/** 좌우 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float TranslateSpeed = 800.0f;

	/** 내려놓는 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float DropSpeed = 600.0f;

	/** Home으로 복귀하는 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ClawMachine|Speed")
	float ReturnSpeed = 1500.0f;

	/* ─────────────────────────────────────────────
	 *  Replicated state
	 * ───────────────────────────────────────────── */

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "ClawMachine|Runtime")
	EClawMachineState State = EClawMachineState::Home;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "ClawMachine|Runtime")
	TObjectPtr<ACSCharacterPlayer> GrabbedPlayer = nullptr;

	/* ─────────────────────────────────────────────
	 *  Runtime (서버 전용)
	 * ───────────────────────────────────────────── */

	FTransform HomeTransform;
	FVector    TargetLocation = FVector::ZeroVector;

	/* ─────────────────────────────────────────────
	 *  Trigger callbacks
	 * ───────────────────────────────────────────── */

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/* ─────────────────────────────────────────────
	 *  State transitions (서버 전용)
	 * ───────────────────────────────────────────── */

	void StartDive(ACSCharacterPlayer* Target);
	void StartRetract();
	void GrabPlayer();
	void ReleasePlayer();

	/** Claw를 Goal로 Speed*DeltaTime만큼 이동. 도달 시 true */
	bool MoveClawTowards(const FVector& Goal, float Speed, float DeltaTime);

	/** Destination(액터 로컬) -> 월드 좌표. 기준은 BeginPlay 시점의 액터 트랜스폼. */
	FVector GetDestinationWorld() const;

	/** Home 복귀 직후 트리거에 남아 있는 플레이어가 있으면 이어서 집는다 (BeginOverlap 이 다시 오지 않으므로) */
	void TryGrabRemainingPlayer();
};
