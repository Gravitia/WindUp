// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSLoginIdLibrary.generated.h"

UCLASS()
class CHRONOSPACE_API UCSLoginIdLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ChronoSpace|Login")
	static FString GetPersistentDeviceLoginId();
};
