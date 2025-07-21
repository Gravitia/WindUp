// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class CHRONOSPACE_API SCSServerTravelWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCSServerTravelWidget) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	FReply OnStageTravelClicked(const FString& StageName);
};
