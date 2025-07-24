// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CSHUD.h"
#include "Engine/Engine.h"

void ACSHUD::BeginPlay() 
{
	Super::BeginPlay();

    return;   // SCSServerTravelWidget 일단 사용안함. 

    if (GEngine && GEngine->GameViewport)
    {
        // Slate 위젯 생성
        ServerTravelWidget = SNew(SCSServerTravelWidget);

        // 게임 뷰포트에 추가
        GEngine->GameViewport->AddViewportWidgetContent(
            ServerTravelWidget.ToSharedRef(),
            1000  // Z-Order (높을수록 앞에 표시)
        );
    }
}
