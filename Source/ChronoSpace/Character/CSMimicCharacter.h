// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/CSCharacterPlayer.h"
#include "CSMimicCharacter.generated.h"

// 분신이 원본의 행동을 따라하는 방식
UENUM(BlueprintType)
enum class EMimicMode : uint8
{
	Mirror   UMETA(DisplayName = "Mirror (그대로 따라함)"),
	Inverted UMETA(DisplayName = "Inverted (반대로 움직임)"),
	Delayed  UMETA(DisplayName = "Delayed (N초 전 행동)"),
};

/**
 * 파란 구역(CSMimicSourceZone)에 들어간 플레이어의 행동을 빨간 구역(CSMimicTargetZone)에서 따라하는 분신.
 * - 서버에서 원본 플레이어의 이동/점프/능력 입력을 미러링한다.
 * - PlayerState가 없으므로 자체 ASC를 가지고, 원본의 능력을 같은 InputID로 복사받는다.
 * - 분신은 죽지 않는다. (SetDead 무시, bIgnoreKillZone)
 */
UCLASS()
class CHRONOSPACE_API ACSMimicCharacter : public ACSCharacterPlayer
{
	GENERATED_BODY()

public:
	ACSMimicCharacter();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

	// PlayerState 기반 ASC 초기화 로직(부모)을 타면 안 된다
	virtual void OnRep_PlayerState() override;

	// 분신은 죽지 않는다
	virtual void SetDead() override;

	// 서버 전용: 원본 플레이어와 존 트랜스폼으로 초기화 (스폰 직후 호출)
	void InitMimic(ACSCharacterPlayer* InSource, const FTransform& InSourceZoneTM, const FTransform& InTargetZoneTM,
		EMimicMode InMode = EMimicMode::Mirror, float InDelaySeconds = 3.0f);

	// 서버 전용: 원본의 능력 입력(InputID)을 미러링 (Delayed 모드에서는 큐에 쌓았다가 지연 적용)
	void MirrorAbilityInput(int32 InputId, bool bPressed);

	FORCEINLINE ACSCharacterPlayer* GetSourceCharacter() const { return SourceCharacter.Get(); }

protected:
	void MirrorMovement();
	void MirrorJump();
	void TickDelayed();
	void ApplyMovement(const FVector& SourceAccel, float MaxAccelIn);
	void ApplyJump(bool bSourceJumpPressed);
	void ApplyAbilityInput(int32 InputId, bool bPressed);
	void CopyAbilitiesFromSource();

	UPROPERTY()
	TObjectPtr<class UAbilitySystemComponent> MimicASC;

	// 원본 플레이어 (서버 전용)
	TWeakObjectPtr<ACSCharacterPlayer> SourceCharacter;

	// 파란 구역 기준 방향을 빨간 구역 기준으로 돌리는 회전
	FQuat MirrorQuat = FQuat::Identity;

	bool bPrevJumpPressed = false;

	// 미러링 방식
	EMimicMode MimicMode = EMimicMode::Mirror;

	// Delayed 모드: 몇 초 전 행동을 재생할지
	float DelaySeconds = 3.0f;

private:
	// Delayed 모드용 입력 기록 (서버 전용)
	struct FMimicInputFrame
	{
		double Time = 0.0;
		FVector Accel = FVector::ZeroVector;
		bool bJumpPressed = false;
	};

	struct FMimicAbilityEvent
	{
		double Time = 0.0;
		int32 InputId = 0;
		bool bPressed = false;
	};

	TArray<FMimicInputFrame> InputHistory;
	TArray<FMimicAbilityEvent> PendingAbilityInputs;
};
