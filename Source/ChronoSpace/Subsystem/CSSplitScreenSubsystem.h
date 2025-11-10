// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CSSplitScreenSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API UCSSplitScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    UCSSplitScreenSubsystem();

    // UGameInstance 오버라이드
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // 스플릿 스크린 기본 기능
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void EnableSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void DisableSplitScreen();

    // 상태 확인 함수들
    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsSplitScreenEnabled() const { return bSplitScreenActive; }

    UFUNCTION(BlueprintPure, Category = "Split Screen")
    bool IsDualModeEnabled() const { return bUseDualMode; }

protected:
    // 스플릿 스크린 설정
    void SetupSplitScreenViewport();

    // 설정 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bUseDualMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    int32 MaxSplitScreenPlayers = 2;

    // 런타임 상태
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bSplitScreenActive = false;

    // 타이머 핸들들
    FTimerHandle SplitScreenTimerHandle;
    FTimerHandle DualModeToggleHandle;
};
