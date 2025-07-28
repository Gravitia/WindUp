// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSSwitchBaseData.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSSwitchBaseData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UCSSwitchBaseData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<class UMaterialInstance> MaterialGlowNonInteracted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<class UMaterialInstance> MaterialGlowInteracted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<class UMaterialInstance> MaterialSolidNonInteracted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<class UMaterialInstance> MaterialSolidInteracted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftClassPtr<UUserWidget> InteractionPromptWidgetClass;
};
