// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WBP_DisplaySetting.h"
#include "GameFramework/GameUserSettings.h"
#include "RHI.h"   //  RHIGetAvailableResolutions 정의 포함

void UWBP_DisplaySetting::NativeConstruct()
{
    Super::NativeConstruct();

    if (ResolutionCombo)
    {
        ResolutionCombo->ClearOptions();
    }

    // GPU/모니터가 지원하는 해상도 탐색
    if (RHIGetAvailableResolutions(AvailableResolutions, false))
    {
        for (const auto& Res : AvailableResolutions)
        {
            FString Label = FString::Printf(TEXT("%dx%d @ %dHz"), Res.Width, Res.Height, Res.RefreshRate);
            if (ResolutionCombo)
            {
                ResolutionCombo->AddOption(Label);
            }
        }

        // 현재 해상도 기본 선택
        if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
        {
            FIntPoint CurrentRes = Settings->GetScreenResolution();

            for (const auto& Res : AvailableResolutions)
            {
                if (Res.Width == CurrentRes.X && Res.Height == CurrentRes.Y)
                {
                    FString MatchLabel = FString::Printf(TEXT("%dx%d @ %dHz"), Res.Width, Res.Height, Res.RefreshRate);
                    int32 FoundIndex = ResolutionCombo->FindOptionIndex(MatchLabel);
                    if (FoundIndex != -1)
                    {
                        ResolutionCombo->SetSelectedOption(MatchLabel);
                    }
                    break;
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("지원 해상도 목록을 가져오지 못했습니다!"));
    }

    if (ApplyButton)
    {
        ApplyButton->OnClicked.AddDynamic(this, &UWBP_DisplaySetting::OnApplyClicked);
    }
}

void UWBP_DisplaySetting::OnApplyClicked()
{
    if (!ResolutionCombo) return;

    FString Selected = ResolutionCombo->GetSelectedOption();
    if (Selected.IsEmpty()) return;

    for (const auto& Res : AvailableResolutions)
    {
        FString Label = FString::Printf(TEXT("%dx%d @ %dHz"), Res.Width, Res.Height, Res.RefreshRate);
        if (Label == Selected)
        {
            if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
            {
                Settings->SetScreenResolution(FIntPoint(Res.Width, Res.Height));

                // 기본은 Borderless Fullscreen (업계 표준)
                // Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
                Settings->SetFullscreenMode(EWindowMode::Windowed);

                Settings->ApplySettings(false);

                UE_LOG(LogTemp, Log, TEXT("해상도 변경: %s"), *Label);
            }
            break;
        }
    }
}