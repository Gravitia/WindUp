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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS") 
	TObjectPtr<class UCSGASManagerComponent> GASManagerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformRecord")
	TObjectPtr<class UCSTransformRecordComponent> TransformRecordComponent;

protected:
	virtual void BeginPlay() override;
	virtual void PreInitializeComponents() override; 
	virtual void SetDead() override;

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

// Input Section
protected:
	void ShoulderMove(const FInputActionValue& Value);
	void ShoulderLook(const FInputActionValue& Value);

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
};
