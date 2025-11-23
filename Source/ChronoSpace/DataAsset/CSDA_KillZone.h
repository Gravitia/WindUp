// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSDA_KillZone.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSDA_KillZone : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ReviveDelay{ 1.0f };
};
