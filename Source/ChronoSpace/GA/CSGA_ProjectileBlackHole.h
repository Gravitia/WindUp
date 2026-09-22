// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/CSGameplayAbility.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "CSGA_ProjectileBlackHole.generated.h"

class ACSBlackHole;
class UCSAT_AimTrace;

// ---- 서버 명령 페이로드 --------------------------------------------------
// 서버 인스턴스도 같은 클래스라 MaxGuideDistance, BlackHoleClass, 중력 파라미터는
// 자기 프로퍼티에서 읽는다. 클라가 보내는 건 조준 방향뿐이다.

USTRUCT()
struct FCSBlackHoleSpawnCmd
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
};

USTRUCT()
struct FCSBlackHoleMoveCmd
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;
};

USTRUCT()
struct FCSBlackHoleReleaseCmd
{
	GENERATED_BODY()
};

/**
 * 조준(클라, LocalOnly, UCSAT_AimTrace) → 명령 릴레이 → 스폰·이동·파괴(서버, OnServerCommand).
 *
 * 우클릭을 누르면 스폰, 잡고 있는 동안 조준 끝점으로 이동, 떼면 파괴하고 어빌리티 종료.
 * 어빌리티 키 자체가 우클릭이라 잡고 있는 동안 재발동은 일어나지 않는다.
 * (예전 "한 번 눌러 조준, 다시 눌러 취소" 토글은 Guide 어빌리티 시절 흔적이라 제거했다.)
 */
UCLASS()
class CHRONOSPACE_API UCSGA_ProjectileBlackHole : public UCSGameplayAbility
{
	GENERATED_BODY()

public:
	UCSGA_ProjectileBlackHole();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnServerCommand(const FInstancedStruct& Payload) override;

	/** 릴리즈(정리) 명령은 사망 중에도 통과시킨다. 정리를 막아서 얻는 게 없고, 막으면 블랙홀이 남는다. */
	virtual bool CanReceiveServerCommand(const FInstancedStruct& Payload) const override;

	/** 서버에서 어빌리티가 제거될 때(캐릭터 교체 등) 남은 블랙홀을 정리한다 */
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	// ---- 디자이너 값 ----
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Aim")
	float MaxGuideDistance = 2000.0f;

	/** 조준 갱신 간격(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Aim")
	float UpdateRate = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Aim")
	float MouseYSensitivity = 1.5f;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|BlackHole")
	float Duration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|BlackHole")
	float GravityInfluenceRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|BlackHole")
	float PullStrength;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|BlackHole")
	float StopRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|BlackHole")
	TSubclassOf<ACSBlackHole> BlackHoleClass;

	bool bCheckMeshComponentPulledByBlackHole{ true };

	/** 블랙홀 조준 시 사용하는 카메라 줌 Ability (BP 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "CSEditable|ProjectileBlackHole|Camera")
	TSubclassOf<UGameplayAbility> CameraZoomAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Camera")
	float CameraZOffsetWhileAiming = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Camera")
	bool bApplyCameraZOffsetWhileAiming = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Camera")
	float CameraOffsetLerpDuration = 0.5f;

	/** 줌 복원(줌아웃) Lerp 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|ProjectileBlackHole|Camera")
	float CameraOffsetRestoreLerpDuration = 0.3f;

	// ---- 클라: 조준 ----
protected:
	UPROPERTY()
	TObjectPtr<UCSAT_AimTrace> AimTask;

	/** 발동 시점의 아바타. 사망하거나 리스폰으로 바뀌면 어빌리티를 끝낸다 (엔진은 아바타가 바뀌어도 EndAbility 를 안 부른다). */
	TWeakObjectPtr<AActor> AimingAvatar;

	FVector CurrentDirection = FVector::ForwardVector;
	FVector CurrentEndLocation = FVector::ZeroVector;

	UFUNCTION()
	void OnAimUpdated(FVector Direction, FVector EndLocation);

	void CheckMouseInput();

	/** 클라(조준 중인 인스턴스) 전용 상태. 서버는 SpawnedBlackHole 유효성으로 판단한다. */
	bool bIsBlackHoleSpawned = false;

	/** 마지막으로 서버에 보낸 조준 끝점. 이동 명령은 이 값에서 유의미하게 바뀌었을 때만 보낸다. */
	FVector LastSentEndLocation = FVector::ZeroVector;

	/** 마지막 이동 명령 전송 시각(월드 초). Unreliable 드랍 복구용 하트비트 기준. */
	float LastMoveSendTime = 0.f;

	/** 끝점이 안 바뀌어도 이 주기로는 한 번 보낸다 */
	static constexpr float MoveHeartbeatInterval = 0.2f;

	// ---- 서버 ----
protected:
	void ServerSpawnBlackHole(const FVector& Direction);
	void ServerMoveBlackHole(const FVector& Direction);
	void ServerReleaseBlackHole();

	/** 아바타(캐릭터)가 파괴되면(사망 리스폰 등) 클라의 릴리즈 명령 없이도 서버가 정리한다 */
	UFUNCTION()
	void OnServerAvatarDestroyed(AActor* DestroyedActor);

	/** 서버 인스턴스가 스폰한 블랙홀. 클라 인스턴스에서는 항상 null. */
	UPROPERTY()
	TObjectPtr<ACSBlackHole> SpawnedBlackHole;

	/** OnServerAvatarDestroyed 를 바인딩한 아바타. 릴리즈 때 해제한다. */
	TWeakObjectPtr<AActor> BoundAvatar;

	// ---- 카메라 ----
protected:
	/**
	 * 카메라 오프셋의 적용/복원과 원본값 보관은 전부 UCSCameraRigComponent 가 한다.
	 * 어빌리티가 타이머로 Lerp 를 돌리면 UGameplayAbility::EndAbility 의
	 * ClearAllTimersForObject(this) 에 지워져 복원이 아예 실행되지 않는다.
	 */
	void ApplyCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo);
	void RestoreCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo);

	class ACSCharacterPlayer* GetCameraRigOwner(const FGameplayAbilityActorInfo* ActorInfo) const;
};
