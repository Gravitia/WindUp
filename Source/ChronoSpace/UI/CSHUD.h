// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SCSServerTravelWidget.h"
#include "CSHUD.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

private:
	TSharedPtr<SCSServerTravelWidget> ServerTravelWidget;
	
};
