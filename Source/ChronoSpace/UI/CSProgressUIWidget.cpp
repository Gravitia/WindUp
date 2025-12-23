// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CSProgressUIWidget.h"

void UCSProgressUIWidget::Show(FName InProgressTextId, float Duration)
{
	// 방어 코드: 잘못된 입력 방지
	if (InProgressTextId.IsNone())
	{
		return;
	}

	if (Duration <= 0.f)
	{
		return;
	}

	// 실제 UI 연출은 블루프린트에 위임
	OnShowProgressUI(InProgressTextId, Duration);
}