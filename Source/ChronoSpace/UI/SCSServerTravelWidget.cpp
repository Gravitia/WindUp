// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/SCSServerTravelWidget.h"
#include "SlateOptMacros.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Input/SButton.h"


// L_Dimension
// L_Gym
// L_Stage_01_01

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCSServerTravelWidget::Construct(const FArguments& InArgs)
{
    ChildSlot
        [
            SNew(SVerticalBox)
                // Stage 1 버튼
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(10)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Go to Stage 1"))
                        .OnClicked_Lambda([this]()
                            {
                                return OnStageTravelClicked("L_TestLevel1");
                            })
                ]
                // Stage 2 버튼
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(10)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Go to Stage 2"))
                        .OnClicked_Lambda([this]()
                            {
                                return OnStageTravelClicked("L_TestLevel2");
                            })
                ]
                // Stage 3 버튼
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(10)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Go to Stage 3"))
                        .OnClicked_Lambda([this]()
                            {
                                return OnStageTravelClicked("L_TestLevel3");
                            })
                ]
                // Stage 4 버튼
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(10)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Go to Stage 4"))
                        .OnClicked_Lambda([this]()
                            {
                                return OnStageTravelClicked("L_TestLevel4");
                            })
                ]
                // Stage 5 버튼
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(10)
                [
                    SNew(SButton)
                        .Text(FText::FromString("Go to Stage 5"))
                        .OnClicked_Lambda([this]()
                            {
                                return OnStageTravelClicked("L_TestLevel5");
                            })
                ]
        ];
}
FReply SCSServerTravelWidget::OnStageTravelClicked(const FString& StageName)
{
    UWorld* World = GEngine->GetCurrentPlayWorld();

    if (World && World->GetNetMode() != NM_Client)
    {
        const FString Command = FString::Printf(TEXT("%s?listen"), *StageName);
        World->ServerTravel(Command);
        UE_LOG(LogTemp, Log, TEXT("Server Travel: %s"), *Command);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Client cannot call ServerTravel"));
    }

    return FReply::Handled();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
