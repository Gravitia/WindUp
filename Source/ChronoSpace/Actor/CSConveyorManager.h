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

	/**
	 * 플레이트가 실제로 흘러가는 방향(월드 단위 벡터).
	 * RiderPushSpeed 가 양수일 때 캐릭터를 미는 방향이기도 하다.
	 *
	 * 플레이트는 매니저 로컬 +Y(Right) 축 위에 늘어서고 진행값이 줄어드는 쪽으로
	 * 이동하므로, MoveSpeed 가 양수면 월드 -Right 가 진행 방향이다.
	 * MoveSpeed 를 음수로 뒤집으면 이 방향도 같이 뒤집힌다.
	 */
	UFUNCTION(BlueprintPure, Category = "Conveyor")
	FVector GetBeltDirection() const;

protected:
	// =========================
	// Conveyor Logic
	// =========================
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Logic")
	float MoveSpeed = 300.f;

	/**
	 * 클라이언트에서만 쓰는 부드럽게 하기 세기. 서버는 계산값을 그대로 쓴다.
	 *
	 * 진행값은 서버 시간에서 유도하는데, 클라의 서버 시간 추정치가 0.1초마다 갱신되면서
	 * 아주 조금씩 튄다. 그 미세한 튐만 흡수하는 용도라 값이 클수록 정확하고 반응이 빠르다.
	 * 너무 낮추면 벨트가 통째로 뒤처져 보이므로 10 아래로는 내리지 말 것.
	 */
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Logic", meta = (ClampMin = "1.0"))
	float ClientSmoothingSpeed = 20.f;

	// =========================
	// Conveyor Rider (가속 / 감속 벨트)
	// =========================
	/**
	 * 벨트에 올라탄 캐릭터에게 매 프레임 더해줄 속도 (cm/s).
	 *
	 *   양수 = 플레이트가 가는 쪽으로 밀어 준다 → 가속 벨트
	 *   음수 = 반대쪽으로 밀어 준다           → 감속 벨트
	 *   0    = 아무것도 하지 않는다 (기존 컨베이어와 완전히 동일)
	 *
	 * 캐릭터 본인의 이동에 "더해지는" 값이다. MaxWalkSpeed 를 건드리지 않으므로
	 * 스프린트 같은 기존 능력과 서로 덮어쓰지 않는다. 가만히 서 있으면 이 속도로 실려 간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|Conveyor|Rider")
	float RiderPushSpeed = 0.f;

	/**
	 * 올라타고 내릴 때 미는 힘이 붙고 빠지는 데 걸리는 시간(초). 0 이면 즉시 적용된다.
	 * 발을 딛는 순간 속도가 툭 튀는 것을 막는다. 점프로 벗어나면 이 시간 동안
	 * 힘이 서서히 빠지므로 관성이 남은 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "CSEditable|Conveyor|Rider", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float RiderBlendTime = 0.2f;

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

	/**
	 * AssignedPlatforms 에 넣은 플랫폼들을 런타임에 놓일 자리에 그대로 배치한다.
	 * 디테일 패널의 버튼으로 누른다.
	 *
	 * 플랫폼의 실제 위치는 런타임에 "매니저 트랜스폼 + (순번 x ConveyorSpacing)" 으로
	 * 매 프레임 덮어써진다. 즉 레벨에서 손으로 옮겨 둔 위치는 게임에 아무 영향이 없고,
	 * 에디터에서 보이는 모습만 실제와 달라진다. 이 버튼은 그 간극을 없앤다.
	 *
	 * 높이는 각 플랫폼의 ZOffset 을 그대로 따른다. 회전은 매니저를 따라간다.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Conveyor", meta = (DisplayName = "Align Platforms"))
	void AlignAssignedPlatforms();

private:
	/**
	 * 지금 이 순간의 진행값을 서버 시간에서 직접 계산한다. [0, TotalLength) 범위.
	 *
	 * 진행값은 시간의 함수일 뿐이라 서버와 클라가 각자 계산해도 같은 값이 나온다.
	 * 예전에는 서버가 매 프레임 진행값을 복제하고 클라가 그걸 쫓아갔는데, 클라의 값이
	 * 항상 왕복 지연만큼 뒤처져서 플레이트 위치가 양쪽에서 어긋났다. 그 위에 선 캐릭터의
	 * 기준 바닥이 서로 다른 자리에 있으니 서버 정정이 들어와 흔들렸다.
	 * 이제는 복제가 아예 없다.
	 *
	 * AGameStateBase::GetServerWorldTimeSeconds 는 클라에서도 0.1초마다 갱신되는
	 * 서버 시계다. GameState 가 아직 없는 접속 직후에는 로컬 시간으로 대신한다.
	 */
	float ComputeProgressFromServerTime() const;

	/** 벨트에 올라탄 캐릭터를 찾아 RiderPushSpeed 만큼 밀어 준다. */
	void UpdateRiderPush(float DeltaSeconds);

	/** 이 액터가 내가 관리하는 플랫폼인가. (캐릭터의 movement base 판정용) */
	bool IsMyPlatform(const AActor* InActor) const;

	float TotalLength = 0.f;

	float TargetProgress = 0.f;
	float SmoothedProgress = 0.f;

	/**
	 * 캐릭터별 현재 블렌드 알파(0~1). 올라타면 1로, 내리면 0으로 향한다.
	 * 0에 닿으면 항목을 지운다. 서버와 해당 캐릭터를 조종하는 머신에서만 채워진다.
	 */
	TMap<TWeakObjectPtr<class ACharacter>, float> RiderBlend;
};
