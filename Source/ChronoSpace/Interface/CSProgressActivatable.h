// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CSProgressActivatable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCSProgressActivatable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class CHRONOSPACE_API ICSProgressActivatable
{
	GENERATED_BODY()


public:
    // BP에서도 구현 가능 + C++에서도 override 가능
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Default|Progress")
    void Activate(AActor* InstigatorActor, FName TriggerId);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Default|Progress")
    void Deactivate(AActor* InstigatorActor, FName TriggerId);
};
