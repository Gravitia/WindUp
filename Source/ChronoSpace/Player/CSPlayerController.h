// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CSPlayerController.generated.h"

class UCSGameUIWidget;
class SCSServerTravelWidget;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ACSPlayerController();

	// UI ���� Ŭ���� - Blueprint���� ����
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UCSGameUIWidget> GameUIWidgetClass;

	// ���� UI ����
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TObjectPtr<UCSGameUIWidget> GameUIWidget;

	// UI ���� �Լ���
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleGameUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshGameUI();

	// ���� UI ���� Ȯ��
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsGameUIVisible() const;

	/*
	// UI ���� ���� ���� (Blueprint��)
	UFUNCTION(BlueprintPure, Category = "UI")
	UCSGameUIWidget* GetGameUIWidget() const { return GameUIWidget; }
	*/

	// =========================
	// Server Travel UI ���� �Լ���
	// =========================

	UFUNCTION(BlueprintCallable, Category = "Server Travel")
	void ShowServerTravelUI();

	UFUNCTION(BlueprintCallable, Category = "Server Travel")
	void HideServerTravelUI();

	UFUNCTION(BlueprintCallable, Category = "Server Travel")
	void ToggleServerTravelUI();

	UFUNCTION(BlueprintPure, Category = "Server Travel")
	bool IsServerTravelUIVisible() const;



	void ShakeCamera();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CameraShake")
	TSubclassOf<class UCameraShakeBase> CameraShake;

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	// PlayerState ��ȭ ����
	virtual void OnRep_PlayerState() override;

private:
	

	void SetupInputMode();
	void CreateGameUI();
	void InitializeUI();

	// Server Travel Slate ���� ����
	void CreateServerTravelWidget();

	// UI ���� ���� Ÿ�̸�
	FTimerHandle UICreationTimerHandle;

// Dual Mode
protected:
	UFUNCTION()
	void UpdateRenderTarget();

	class ACSCharacterPlayer* FindFirstOtherPawn();

	class ASceneCapture2D* SpawnCaptureAndAttach(class UCameraComponent* TargetCam, UTextureRenderTarget2D* TargetRT);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
	TObjectPtr<UTextureRenderTarget2D> RenderTargetP0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
	TObjectPtr<UTextureRenderTarget2D> RenderTargetP1;

	UPROPERTY()
	TObjectPtr<class ASceneCapture2D> CaptureP0;

	UPROPERTY()
	TObjectPtr<class ASceneCapture2D> CaptureP1;

// Dual Mode UI
protected:
	void ToggleDualMode();
	void OpenDualMode();
	void CloseDualMode();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dual Mode UI")
	TSubclassOf<class UUserWidget> DualModeUIClass;

	UPROPERTY()
	TObjectPtr<class UUserWidget> DualModeUI;

	bool bIsDualMode;

// Server Travel Slate UI ����
	TSharedPtr<SCSServerTravelWidget> ServerTravelWidget;

};
