// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSPlayerInteractionComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCSPlayerInteractionComponent();

	void SetTrigger(class UCapsuleComponent* InTrigger);
	void SetInteractionInputComponent(class UEnhancedInputComponent* InputComponent);

protected:
	virtual void BeginPlay() override;

protected:
	void InteractionInputPressed();

	UFUNCTION(Server, Reliable)
	void ServerInteractionInputPressed();

	void HandleInteractionInputPressed();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> InteractAction;

private:
	UFUNCTION()
	void OnComponentBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);
	
	UFUNCTION()
	void OnComponentEndOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ClearCachedActors();

	UPROPERTY()
	TObjectPtr<class UCapsuleComponent> Trigger;

	UPROPERTY()
	TObjectPtr<AActor> CurrentInteractionActor;

	UPROPERTY()
	TMap<FName, TObjectPtr<AActor>> InteractionActors;
};
