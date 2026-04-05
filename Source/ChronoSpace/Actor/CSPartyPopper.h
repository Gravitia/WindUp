// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/CSInteractionInterface.h"
#include "CSPartyPopper.generated.h"

class USplineMeshComponent;

UCLASS()
class CHRONOSPACE_API ACSPartyPopper : public AActor, public ICSInteractionInterface
{
	GENERATED_BODY()

public:
	ACSPartyPopper();

	// ICSInteractionInterface
	virtual void BeginInteraction() override;
	virtual void EndInteraction()   override;
	virtual void Interact()         override;

	/** 외부(버튼 BP 등)에서 직접 폭발 트리거 — 서버 권한 필요 */
	UFUNCTION(BlueprintCallable, Category="Default")
	void Explode();

	/** 에디터 뷰포트에서도 Tick 허용 (원뿔 미리보기용) */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* ════════════════════════════════════
	 *  컴포넌트
	 * ════════════════════════════════════ */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 폭발 시 날아가는 뚜껑 메쉬 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	/** 파티클 발사 위치 (폭죽 앞단) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<USceneComponent> LaunchPoint;

	/** 스플라인 줄 연결 위치 (폭죽 손잡이 끝단) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<USceneComponent> EndPoint;

	/** 버튼과의 연결 줄 (SplineMesh — 물리 없이 고정된 곡선) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<USplineMeshComponent> StringMesh;

	/* ════════════════════════════════════
	 *  에디터 설정
	 * ════════════════════════════════════ */

	/** 5가지 입자 메쉬 타입 (에디터에서 할당) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	TArray<TObjectPtr<UStaticMesh>> ParticleMeshTypes;

	/** 생성할 입자 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default", meta=(ClampMin=1, ClampMax=200))
	int32 ParticleCount = 50;

	/** 폭발 기본 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float ExplosionSpeed = 600.0f;

	/** 속도 랜덤 범위 최솟값 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default", meta=(ClampMin=0.1f, ClampMax=1.0f))
	float SpeedRandomMin = 0.5f;

	/** 속도 랜덤 범위 최댓값 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default", meta=(ClampMin=1.0f, ClampMax=3.0f))
	float SpeedRandomMax = 1.5f;

	/** 입자 생존 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float ParticleLifeTime = 3.0f;

	/** 중력 가속도 (cm/s²) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float GravityZ = 980.0f;

	/** 입자 초기 스케일 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float ParticleScale = 1.0f;

	/** 입자 회전 속도 최댓값 (도/초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float MaxRotationSpeed = 720.0f;

	/** 컨페티 발사 방향 (액터 로컬 스페이스) — X=앞, Z=위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	FVector LaunchDirection = FVector(1.0f, 0.0f, 0.0f);

	/** 원뿔 반각 (도) — 클수록 넓게 퍼짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default", meta=(ClampMin=1.0f, ClampMax=90.0f))
	float ConeHalfAngle = 30.0f;

	/** true 이면 한 번만 폭발 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	bool bOneShot = true;

	/** 폭발 시 재생할 사운드 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 뚜껑 발사 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float LidLaunchSpeed = 500.0f;

	/** 거리 트리거 대상 버튼 액터 (레벨에서 인스턴스별로 설정) */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Default")
	TObjectPtr<AActor> LinkedButtonActor;

	/** 버튼과의 거리가 이 값 이상이면 폭발 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Default")
	float TriggerDistance = 150.0f;

	/** 연결 줄 처짐 정도 (cm) — 클수록 아래로 더 처짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float StringSag = 30.0f;

	/** 줄 끝점 오프셋 — 버튼 로컬 스페이스 기준 (버튼의 특정 위치에 연결) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Default")
	FVector StringEndOffset = FVector::ZeroVector;

	/** 폭발 시 본체가 날아가는 방향 (액터 로컬 스페이스, 초록=Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	FVector RecoilDirection = FVector(0.0f, 1.0f, 0.0f);

	/** 폭발 시 본체가 날아가는 초기 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default")
	float RecoilSpeed = 400.0f;

	/** 폭발 후 컨페티·뚜껑 발사까지 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default", meta=(ClampMin=0.0f, ClampMax=5.0f))
	float LaunchDelay = 0.7f;

private:
	/* ════════════════════════════════════
	 *  런타임 입자 데이터
	 * ════════════════════════════════════ */

	struct FParticleState
	{
		FVector  Velocity        = FVector::ZeroVector;
		FRotator AngularVelocity = FRotator::ZeroRotator;
		float    Elapsed         = 0.0f;
	};

	struct FLidState
	{
		FVector  Velocity        = FVector::ZeroVector;
		FRotator AngularVelocity = FRotator::ZeroRotator;
		float    Elapsed         = 0.0f;
		bool     bFlying         = false;
	};

	struct FRecoilState
	{
		FVector Velocity = FVector::ZeroVector;
		bool    bActive  = false;
	};


	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ParticleComps;

	TArray<FParticleState> ParticleStates;

	// 딜레이 발사를 위해 Multicast 수신 데이터 임시 보관
	TArray<FVector> PendingVelocities;
	TArray<int32>   PendingMeshIndices;
	FTimerHandle    LaunchTimerHandle;

	FLidState    LidState;
	FRecoilState RecoilState;
	FTransform   InitialLidRelTransform;

	bool bSimulating     = false;
	bool bHasFired       = false;
	bool bIsExploding    = false;
	bool bLaunchPending  = false;  // DoLaunch 타이머 대기 중

	/* ════════════════════════════════════
	 *  네트워크
	 * ════════════════════════════════════ */

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Explode(const TArray<FVector>& Velocities, const TArray<int32>& MeshIndices);

	/* ════════════════════════════════════
	 *  내부 함수
	 * ════════════════════════════════════ */

	void DoExplode(const TArray<FVector>& Velocities, const TArray<int32>& MeshIndices);
	void DoLaunch();   // LaunchDelay 후 실제 컨페티·뚜껑 발사
	void TickLid(float DeltaTime);
	void TickParticles(float DeltaTime);
	void TickRecoil(float DeltaTime);
	void CleanupParticles();
	void UpdateStringMesh();
};
