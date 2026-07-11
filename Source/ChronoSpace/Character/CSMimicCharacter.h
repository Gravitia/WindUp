// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/CSCharacterPlayer.h"
#include "CSMimicCharacter.generated.h"

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
	void InitMimic(ACSCharacterPlayer* InSource, const FTransform& InSourceZoneTM, const FTransform& InTargetZoneTM);

	// 서버 전용: 원본의 능력 입력(InputID)을 그대로 적용
	void MirrorAbilityInput(int32 InputId, bool bPressed);

	FORCEINLINE ACSCharacterPlayer* GetSourceCharacter() const { return SourceCharacter.Get(); }

protected:
	void MirrorMovement();
	void MirrorJump();
	void CopyAbilitiesFromSource();

	UPROPERTY()
	TObjectPtr<class UAbilitySystemComponent> MimicASC;

	// 원본 플레이어 (서버 전용)
	TWeakObjectPtr<ACSCharacterPlayer> SourceCharacter;

	// 파란 구역 기준 방향을 빨간 구역 기준으로 돌리는 회전
	FQuat MirrorQuat = FQuat::Identity;

	bool bPrevJumpPressed = false;
};
