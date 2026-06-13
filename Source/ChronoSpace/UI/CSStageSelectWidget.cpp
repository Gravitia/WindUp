// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CSStageSelectWidget.h"
#include "Actor/CSStagePortal.h"
#include "Player/CSPlayerController.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCSStageButtonHandler::HandleClicked()
{
    if (UCSStageSelectWidget* W = Owner.Get())
    {
        W->ConfirmSelection(Chapter, Stage);
    }
}

void UCSStageSelectWidget::Setup(ACSStagePortal* InPortal, const TArray<FCSStageOption>& InOptions)
{
    OwningPortal = InPortal;
    Options = InOptions;

    // If the frame already exists (re-show), fill it now; otherwise NativeConstruct will.
    PopulateOptions();
}

TSharedRef<SWidget> UCSStageSelectWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }
    if (!WidgetTree->RootWidget)
    {
        BuildFrame();
    }
    return Super::RebuildWidget();
}

void UCSStageSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();
    PopulateOptions();
}

void UCSStageSelectWidget::BuildFrame()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Full-screen dim behind the panel.
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
    Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
    if (UCanvasPanelSlot* DimSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Dim)))
    {
        DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        DimSlot->SetOffsets(FMargin(0.0f));
    }

    // Centered panel.
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.96f));
    Panel->SetPadding(FMargin(24.0f));
    if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Root->AddChild(Panel)))
    {
        PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
        PanelSlot->SetSize(FVector2D(780.0f, 560.0f));
    }

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
    Panel->SetContent(Column);

    // Title.
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
    Title->SetText(FText::FromString(TEXT("스테이지 선택")));
    Title->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
    {
        TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
        TitleSlot->SetHorizontalAlignment(HAlign_Center);
    }

    // Scrollable area holding the wrapping grid of stage buttons.
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
    if (UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll))
    {
        ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    StageContainer = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("StageContainer"));
    Scroll->AddChild(StageContainer);
}

void UCSStageSelectWidget::PopulateOptions()
{
    if (!StageContainer) return;

    StageContainer->ClearChildren();
    ButtonHandlers.Reset();

    for (const FCSStageOption& Option : Options)
    {
        // Uniform tile so normal (1 button) and debug (many) layouts both look tidy.
        USizeBox* Tile = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Tile->SetWidthOverride(150.0f);
        Tile->SetHeightOverride(64.0f);

        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        Btn->SetIsEnabled(Option.bSelectable);
        if (Option.bIsNextStage)
        {
            Btn->SetBackgroundColor(FLinearColor(0.12f, 0.55f, 0.22f, 1.0f)); // highlight the normal next stage
        }
        Tile->SetContent(Btn);

        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(Option.DisplayName);
        Label->SetJustification(ETextJustify::Center);
        Btn->SetContent(Label);

        UCSStageButtonHandler* Handler = NewObject<UCSStageButtonHandler>(this);
        Handler->Owner = this;
        Handler->Chapter = Option.Chapter;
        Handler->Stage = Option.Stage;
        Btn->OnClicked.AddDynamic(Handler, &UCSStageButtonHandler::HandleClicked);
        ButtonHandlers.Add(Handler);

        if (UWrapBoxSlot* WSlot = StageContainer->AddChildToWrapBox(Tile))
        {
            WSlot->SetPadding(FMargin(6.0f));
        }
    }
}

void UCSStageSelectWidget::ConfirmSelection(int32 Chapter, int32 Stage)
{
    ACSStagePortal* Portal = OwningPortal.Get();
    if (!Portal) return;

    if (Portal->HasAuthority())
    {
        // Host's local player: act directly on the server.
        Portal->HandleTravelRequest(Chapter, Stage);
    }
    else if (ACSPlayerController* PC = Cast<ACSPlayerController>(GetOwningPlayer()))
    {
        // Client: route through an owned actor so the Server RPC is valid.
        PC->ServerRequestStagePortalTravel(Portal, Chapter, Stage);
    }
}
