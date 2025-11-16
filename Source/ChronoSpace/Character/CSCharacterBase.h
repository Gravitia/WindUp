// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CSCharacterBase.generated.h"

UCLASS()
class CHRONOSPACE_API ACSCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACSCharacterBase();

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetDead();

	UFUNCTION(BlueprintCallable)
	virtual void SetRevive();

protected:
	UPROPERTY()
	TObjectPtr<class UCSCustomGravityDirComponent> CustomGravityDirComponent;


	// Wind Up Key
protected:
	void AttachWindUpKeyToSocket();

	UPROPERTY()
	TObjectPtr<AActor> WindUpKeyActor;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr< class UCSVFXComponent > VFXComponent;
}; 
