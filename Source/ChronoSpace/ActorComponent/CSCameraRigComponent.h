// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSCameraRigComponent.generated.h"

/**
 * 모디파이어 블렌드 곡선.
 *
 * FCSCameraModifier 가 USTRUCT 가 아니고 AddModifier 도 BP 노출이 아니라서 UENUM 이 아니다.
 * BP 에서 쓸 일이 생기면 구조체·함수와 함께 한꺼번에 노출한다.
 */
enum class ECSCameraBlend : uint8
{
	/** 등속. 블렌드 시간을 거리/속도로 잡으면 예전 ZoomCamera 와 같은 움직임이 된다 */
	Linear,

	/** 예전 UCSGA_ProjectileBlackHole 의 Lerp 와 같은 커브 */
	EaseInOut,
};

/**
 * 모디파이어 등록/해제에 쓰는 Source 키.
 *
 * 문자열을 호출부에 직접 쓰지 않는다. 등록과 해제의 키가 한 글자라도 어긋나면
 * 모디파이어가 영영 안 빠져서 카메라가 돌아오지 않는다.
 */
namespace CSCameraRigSource
{
	inline const FName ZoomVolume(TEXT("ZoomVolume"));
	inline const FName CameraZoomAbility(TEXT("CameraZoomAbility"));
	inline const FName BlackHoleAim(TEXT("BlackHoleAim"));
}

namespace CSCameraRig
{
	/**
	 * 속도(유닛/초) 기반 요청을 블렌드 시간으로 바꾼다.
	 *
	 * @param ZoomSpeed  예전 ZoomCamera 와 같은 단위 - 실제 속도는 x100 유닛/초
	 * @return           0 이하의 ZoomSpeed 면 -1 (= 움직이지 않음). 예전 동작과 같다
	 */
	CHRONOSPACE_API float BlendTimeFromSpeed(float Distance, float ZoomSpeed);
}

/**
 * 카메라 붐에 걸리는 효과 하나.
 *
 * 요청자는 "Base 대비 얼마나 밀지" 만 등록하고, 최종값 계산과 SpringArm 쓰기는
 * UCSCameraRigComponent 가 한다. 원본값(Base)을 컴포넌트만 들고 있으므로 요청자
 * (볼륨, 어빌리티)의 수명이 어떻게 끊기든 원상 복구가 보장된다.
 */
struct FCSCameraModifier
{
	/** 등록/해제 키 */
	FName Source;

	/** Base 대비 팔 길이 증감 (음수 = 카메라가 가까워진다) */
	float ArmLengthDelta = 0.f;

	/** Base 대비 붐 부착점 오프셋 (부모 로컬 공간) */
	FVector BoomOffsetDelta = FVector::ZeroVector;

	/**
	 * 초. 0 이면 즉시.
	 *
	 * BlendInTime 이 음수면 적용이 아예 안 일어난다 (예전 ZoomSpeed 0 과 같은 의미).
	 * BlendOutTime 은 음수여도 즉시 해제된다 - 해제를 얼리면 모디파이어가 영영 안 빠진다.
	 */
	float BlendInTime = 0.2f;
	float BlendOutTime = 0.2f;

	ECSCameraBlend Blend = ECSCameraBlend::Linear;

	// ── 런타임 상태 ──
	float Alpha = 0.f;
	bool bReleasing = false;
};

/**
 * 캐릭터 SpringArm 의 단독 소유자.
 *
 * 줌 볼륨, 카메라 줌 어빌리티, 블랙홀 조준 오프셋이 각각 Source 를 갖는 모디파이어로
 * 등록되고, 매 Tick 합산된 결과가 SpringArm 에 기록된다.
 *
 * 어빌리티가 아니라 여기서 돌리는 이유: UGameplayAbility::EndAbility 가
 * ClearAllTimersForObject(this) 를 부르기 때문에, 어빌리티가 EndAbility 안에서 건
 * 복원 타이머는 즉시 삭제된다. 컴포넌트 Tick 은 어빌리티 수명과 무관하다.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSCameraRigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSCameraRigComponent();

	/**
	 * SpringArm 과 원본값을 등록한다. 리스폰/재빙의로 다시 불릴 수 있으며,
	 * 그때는 Base 를 다시 잡고 이전 수명의 모디파이어를 버린다.
	 * 호출 전에 SpringArm 이 원본 상태여야 한다 (ACSCharacterPlayer::SetData 참고).
	 */
	void Init( class USpringArmComponent* SpringArm, float InOrgLength );

	/** 같은 Source 가 이미 있으면 목표만 갱신한다. 현재 Alpha 는 유지되므로 카메라가 튀지 않는다 */
	void AddModifier( FCSCameraModifier InModifier );

	/** BlendOut 을 시작한다. Alpha 가 0 이 되면 목록에서 빠지고 Base 로 복귀한다 */
	void RemoveModifier( FName Source );

	/** 블렌드 없이 전부 해제하고 즉시 Base 로 되돌린다 */
	void ClearModifiers();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY()
	TObjectPtr<class USpringArmComponent> CameraBoom;

	/** 모디파이어가 하나도 없을 때의 SpringArm 상태. 이 값의 소유자는 이 컴포넌트뿐이다 */
	float OrgLength{ 0.f };
	FVector BaseBoomOffset{ FVector::ZeroVector };

	TArray< FCSCameraModifier > Modifiers;

private:
	/** Base + 활성 모디파이어 합산 결과를 SpringArm 에 기록한다 */
	void ApplyToSpringArm();

	bool bIsInit{ false };
};
