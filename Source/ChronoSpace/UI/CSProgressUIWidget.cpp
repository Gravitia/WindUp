// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CSProgressUIWidget.h"

void UCSProgressUIWidget::Show(FName InProgressText, float Duration, bool bProgressText)
{
	// 실제 UI 연출은 블루프린트에 위임

	if (bProgressText)
	{
		OnShowProgressUI(InProgressText, Duration);
	}

	OnShowProgressEvent();
}