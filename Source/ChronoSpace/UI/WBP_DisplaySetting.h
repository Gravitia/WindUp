// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "WBP_DisplaySetting.generated.h"



/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UWBP_DisplaySetting : public UUserWidget
{
	GENERATED_BODY()
	
public:
    virtual void NativeConstruct() override;

protected:
    /** 해상도 선택 콤보박스 */
    UPROPERTY(meta = (BindWidget))
    UComboBoxString* ResolutionCombo;

    /** 적용 버튼 */
    UPROPERTY(meta = (BindWidget))
    UButton* ApplyButton;

    /** GPU/모니터가 지원하는 해상도 목록 */
    // UPROPERTY() 
    FScreenResolutionArray AvailableResolutions;

    /** 적용 버튼 눌렀을 때 실행 */
    UFUNCTION()
    void OnApplyClicked();
};
