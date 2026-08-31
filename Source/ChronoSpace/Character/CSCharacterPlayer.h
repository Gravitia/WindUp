// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/CSCharacterBase.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "CSF_CharacterFrameData.h"
#include "CSCharacterPlayer.generated.h"

//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractionDelegate);

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSCharacterPlayer : public ACSCharacterBase, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ACSCharacterPlayer();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void OnRep_PlayerState() override;
	virtual void Landed(const FHitResult& Hit) override;

	/**
	 * IMC 는 폰이 아니라 LocalPlayer 에 붙는다.
	 *
	 * 그래서 "한 컨트롤러가 다른 몸으로 갈아타는" 상황(디버그 캐릭터 스왑)에서는 폰이 바뀔 때마다
	 * 매핑도 같이 갈아끼워야 한다. 원래는 ACSCharacterPlayer::BeginPlay 가 폰당 한 번만 등록해서,
	 * 나중에 빙의된 몸은 자기 IMC 를 등록할 기회가 없었다.
	 *
	 * PawnClientRestart(빙의) 에서 등록하고 UnPossessed(해제) 에서 해제한다. 둘 다 엔진이
	 * 빙의 경로에서 반드시 불러 주는 지점이라, 정상 플레이든 디버그 스왑이든 같은 코드가 처리한다.
	 */
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;

	/**
	 * 이 캐릭터의 InputMappingContext 를 ForController 의 LocalPlayer 에 등록/해제한다.
	 *
	 * 서버가 들고 있는 원격 플레이어의 복사본처럼 GetLocalPlayer() 가 null 인 머신에서는
	 * 조용히 아무것도 하지 않는다. 입력은 조작하는 머신에서만 의미가 있기 때문이다.
	 */
	void ApplyInputMappingContext(class APlayerController* ForController, bool bAdd);

	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE class UCSGASManagerComponent* GetGASManagerComponent() const { return GASManagerComponent; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS") 
	TObjectPtr<class UCSGASManagerComponent> GASManagerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformRecord")
	TObjectPtr<class UCSTransformRecordComponent> TransformRecordComponent;

protected:
	virtual void BeginPlay() override;
	virtual void PreInitializeComponents() override; 
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// 로컬 연출 (입력 잠금/VFX/사운드) - 모든 머신에서 bIsDead 복제로 실행
	virtual void HandleDead() override;
	virtual void HandleRevive() override;

protected:
	UPROPERTY()
	TObjectPtr<class UCSCharacterPushedComponent> PushedComponent;

	UPROPERTY()
	TObjectPtr<class UCSCharacterPulledByBlackhole> PulledByBlackholeComponent;

// Data
protected:
	void SetData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr< class UCSCharacterPlayerData > Data;

// Camera Section
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> FollowCamera;

	UPROPERTY()
	TObjectPtr<class UCSCameraZoomComponent> ZoomComponent;

// Move & Look
public:
	void ZoomCamera( float ZoomLength, float ZoomSpeed );

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	void ShoulderMove(const FInputActionValue& Value);
	void ShoulderLook(const FInputActionValue& Value);

	bool bIsFirstLook;

// Input Section
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> ShoulderMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> ShoulderLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> AbilityPreviewAction;



// ASC Section
protected:
	UPROPERTY(EditAnywhere, Category = GAS)
	TObjectPtr<class UAbilitySystemComponent> ASC;

	/*
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCSGASWidgetComponent> EnergyBar;

	*/
	
// Trigger Section
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCapsuleComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCSPushingCharacterComponent> PushingCharacterComponent;

// WhiteHall
public:
	FORCEINLINE class ACSWhiteHall* GetWhiteHall() { return WhiteHall; }
	void SetWhiteHall(class ACSWhiteHall* InWhiteHall) { WhiteHall = InWhiteHall; }
	void ClearWhiteHall();

// Misc
protected:
	UPROPERTY()
	TObjectPtr<class ACSWhiteHall> WhiteHall;

// Interaction Section
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCSPlayerInteractionComponent> InteractionComponent;

// Character Scaling 

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCSCharacterScaleComponent> ScaleComponent;


// HP UI
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RequestUIRefresh();

// Auto ClockUnwindDOT
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|PassiveEffect")
	TSubclassOf<class UGameplayEffect> ClockUnwindEffect;

/* Character Movemenet Origin Value Save */

// Dash
public:
	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 500.0f;  // default and init DataAsset 

	UPROPERTY(EditAnywhere, Category = "Movement")
	float DashSpeed = 900.0f; //  default and init DataAsset 

// GravityScale
	UPROPERTY(EditAnywhere, Category = "Movement")
	float GravityScale = 2.0f; //  default and init DataAsset 


// Scale
	UPROPERTY(EditAnywhere, Category = "Capsule")
	float BaseCapsuleRadius = 34.0f;

	UPROPERTY(EditAnywhere, Category = "Capsule")
	float BaseCapsuleHalfHeight = 88.0f;


// UnwindUp RPC

private:
	void AlwaysClockUnwind();

protected:
	UFUNCTION(Server, Reliable)
	void Server_ApplyClockUnwind();

	// 새로 추가할 Multicast 함수 선언
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyClockUnwind();

private:
	void ApplyClockUnwind_Internal();
	


// ─────────── Coyote-Time(코요테 점프) ───────────
	UFUNCTION(BlueprintCallable)
	void StartCoyoteTimer();

	UFUNCTION(BlueprintCallable)
	void DisableCoyoteTime();

	UPROPERTY()
	bool bCanCoyoteJump = false;

	UPROPERTY(EditAnywhere)
	float CoyoteTime = 0.33f;          // 관용 구간(초)

	UPROPERTY(EditAnywhere)
	FTimerHandle CoyoteTimerHandle;

	virtual bool CanJumpInternal_Implementation() const override;
	virtual void Falling() override;
	virtual void OnJumped_Implementation() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMode,
		uint8 PrevCustomMode = 0) override;


// Gravity Core
public:
	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastMakeGravityCoreSphere(float SphereRaduis, float SphereScale);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastDestroyGravityCoreSphere();

protected:
	UPROPERTY(Transient)
	TObjectPtr<class USphereComponent> GravityCoreSphere;

// Black Hole
// GA가 RPC가 없는 것에 대한 우회..
public:
	UFUNCTION(Server, Reliable)
	void ServerSpawnAndSetBlackHole(TSubclassOf<class ACSBlackHole> BlackHoleClass,
		FVector Direction, float MaxDistance, float Duration, float GravityInfluenceRange, float PullStrength,
		float StopRange, bool bCheckComponent);

	UFUNCTION(Server, Unreliable)
	void ServerSetBlackHoleLocation(FVector Direction, float MaxDistance);

	UFUNCTION(Server, Reliable)
	void ServerDestoryBlackHole();

	UPROPERTY(Replicated)
	TObjectPtr<class ACSBlackHole> BlackHole;

public:
	float GetReviveTime();

	// 리스폰으로 새로 스폰된 Pawn 에서 부활 연출(사운드) 재생 - 서버가 RespawnSinglePlayer 에서 호출.
	// (구 Pawn 은 같은 틱에 파괴되어 bIsDead=false 복제가 클라에 닿지 않으므로 구 Pawn 의 SetRevive 로는 연출이 호스트에만 남는다)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayReviveEffects();


	/* Respawn Sound */
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* ReviveSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* DeathSound;

public:
	// KillZone Debug Immortal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|KillZone")
	bool bIgnoreKillZone = false;


};
