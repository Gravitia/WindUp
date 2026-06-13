// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/CSStageDataSettings.h"
#include "CSStageSelectWidget.generated.h"

class ACSStagePortal;
class UWrapBox;
class UCSStageSelectWidget;

/**
 * Tiny per-button click router. Each stage button gets its own handler that
 * remembers which (Chapter, Stage) it represents, since UButton::OnClicked
 * carries no payload of its own.
 */
UCLASS()
class UCSStageButtonHandler : public UObject
{
    GENERATED_BODY()

public:
    TWeakObjectPtr<UCSStageSelectWidget> Owner;
    int32 Chapter = 0;
    int32 Stage = 0;

    UFUNCTION()
    void HandleClicked();
};

/**
 * Stage-select UI a portal opens when players gather.
 *
 * The entire layout and button list are built in C++ (RebuildWidget /
 * PopulateOptions), so no UMG design work is required — assign this class (or a
 * Blueprint reparented to it) to the portal and it just works. The portal feeds
 * the destination list via Setup(); clicking a button routes the choice to the
 * server through the portal.
 */
UCLASS()
class CHRONOSPACE_API UCSStageSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Called by the portal right after creation, before AddToViewport. */
    void Setup(ACSStagePortal* InPortal, const TArray<FCSStageOption>& InOptions);

    /** Invoked by a stage button; forwards the choice to the portal/server. */
    UFUNCTION(BlueprintCallable, Category = "Stage Select")
    void ConfirmSelection(int32 Chapter, int32 Stage);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

    /** Builds the static frame (dim background, panel, title, scrollable grid). */
    void BuildFrame();

    /** Fills the grid with one button per option (clears any previous buttons). */
    void PopulateOptions();

    UPROPERTY(Transient)
    TObjectPtr<UWrapBox> StageContainer;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCSStageButtonHandler>> ButtonHandlers;

    TArray<FCSStageOption> Options;
    TWeakObjectPtr<ACSStagePortal> OwningPortal;
};
