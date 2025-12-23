// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CSProgressUIWidget.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSProgressUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 트리거에서 호출하는 진입 함수 */
	UFUNCTION(BlueprintCallable)
	void Show(FName InProgressTextId, float Duration);

protected:
	/** BP에서 구현할 실제 표시 로직 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnShowProgressUI(FName InProgressTextId, float Duration);
};
