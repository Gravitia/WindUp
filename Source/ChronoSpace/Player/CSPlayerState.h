// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "CSPlayerState.generated.h"

UENUM(BlueprintType)
enum class ECSPlayerSlot : uint8
{
	Player0,
	Player1
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, Health, float, MaxHealth);

class ACSRespawnPoint;



/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ACSPlayerState();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Health 占쏙옙占쏙옙 占쌉쇽옙占쏙옙
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthPercent() const;

	// 占쏙옙占쏙옙占쏙옙占쏙옙트
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(Replicated)
	ACSRespawnPoint* PersonalRespawnPoint;

	UFUNCTION()
	void SetPersonalRespawnPoint(ACSRespawnPoint* NewPoint);
	ACSRespawnPoint* GetPersonalRespawnPoint() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = GAS)
	TObjectPtr<class UAbilitySystemComponent> ASC;

	UPROPERTY()
	TObjectPtr<class UCSAttributeSet> AttributeSet;

	virtual void BeginPlay() override;

	// Health 占쏙옙화 占쌥뱄옙
	virtual void HealthChanged(const FOnAttributeChangeData& Data);
	virtual void MaxHealthChanged(const FOnAttributeChangeData& Data);

// Character Mesh
public:
	void SetPlayerSlot(ECSPlayerSlot InSlot) { PlayerSlot = InSlot; }
	ECSPlayerSlot GetPlayerSlot() const { return PlayerSlot; }

	/** SeamlessTravel 시 새 PlayerState 로 PlayerSlot 을 넘긴다. */
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UFUNCTION()
	void OnRep_PlayerSlot();

	DECLARE_EVENT_OneParam(ACSPlayerState, FPlayerSlotChanged, ECSPlayerSlot);
	FPlayerSlotChanged OnPlayerSlotChanged;

protected:
	// Replicated so clients can read who-is-who (e.g. HUD, character-tinted UI).
	// Authority is the server; UCSPlayerSlotSubsystem persists the per-connection
	// assignment across non-seamless ServerTravel so the slot stays stable.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PlayerSlot)
	ECSPlayerSlot PlayerSlot = ECSPlayerSlot::Player0;
};
