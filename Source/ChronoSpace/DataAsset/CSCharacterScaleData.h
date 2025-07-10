// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSCharacterScaleData.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSCharacterScaleData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UCSCharacterScaleData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Small")
	float ScaleSmall;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Small")
	float RadiusSmall;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Small")
	float HalfHeightSmall;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal")
	float ScaleNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal")
	float RadiusNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Normal")
	float HalfHeightNormal;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large")
	float ScaleLarge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large")
	float RadiusLarge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large")
	float HalfHeightLarge;
};
