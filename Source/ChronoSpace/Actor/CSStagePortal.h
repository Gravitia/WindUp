// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Settings/CSStageDataSettings.h"
#include "CSStagePortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACharacter;
class UCSGameProgressSubsystem;
class UCSStageSelectWidget;

/**
 * Co-op stage exit portal.
 *
 * Waits for RequiredPlayerCount players to enter the trigger, then opens a
 * stage-select UI built from the UCSStageDataSettings table:
 *   - Normal play: only the next stage (from CurrentChapter/CurrentStage) is offered.
 *   - Non-shipping with debug unlock on: every stage is offered.
 *
 * When a player confirms a choice, the server marks the current stage cleared
 * (optional), records last-played, fades out, and ServerTravels to the chosen
 * stage's level. The destination level is resolved from the data table, so the
 * portal itself holds no hardcoded target map.
 */
UCLASS()
class CHRONOSPACE_API ACSStagePortal : public AActor
{
    GENERATED_BODY()

public:
    ACSStagePortal();

    // Server-side entry point for an accepted stage choice (validates, then travels).
    // Called directly by the host's widget and via ACSPlayerController for clients.
    void HandleTravelRequest(int32 Chapter, int32 Stage);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnRep_PlayerCount();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastOpenStageSelect();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastCloseStageSelect();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastBeginTransition();

    void ApplyLightVisibility();

    // Local-client UI helpers.
    void OpenStageSelectUI();
    void CloseStageSelectUI();
    void RemoveStageSelectWidget();

    // Builds the list of destinations for the local build/config from the data table.
    void BuildStageOptions(TArray<FCSStageOption>& OutOptions) const;
    bool ShouldAllowAllStages() const;

    void DoServerTravel();
    UCSGameProgressSubsystem* GetProgressSubsystem() const;

protected:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Components")
    TObjectPtr<UStaticMeshComponent> Light1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Components")
    TObjectPtr<UStaticMeshComponent> Light2;

    // === This portal's location in progression (where "here" is) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Stage", meta = (ClampMin = "1"))
    int32 CurrentChapter = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Stage", meta = (ClampMin = "1"))
    int32 CurrentStage = 1;

    // If true, marks (CurrentChapter, CurrentStage) cleared before traveling.
    // Set false for rest/hub stages that shouldn't count toward progression.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Stage")
    bool bMarkCurrentStageCleared = true;

    // === UI ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|UI")
    TSubclassOf<UCSStageSelectWidget> StageSelectWidgetClass;

    // === Travel ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Travel", meta = (ClampMin = "0.0"))
    float FadeDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Travel", meta = (ClampMin = "1", ClampMax = "4"))
    int32 RequiredPlayerCount = 2;

    // === Debug ===
    // When true (and not a Shipping build), the UI offers every stage for free travel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Debug")
    bool bDebugUnlockAllStages = true;

    // === Runtime State ===
    UPROPERTY(ReplicatedUsing = OnRep_PlayerCount)
    int32 PlayerCount = 0;

    UPROPERTY()
    TArray<TObjectPtr<ACharacter>> PlayersInTrigger;

    UPROPERTY()
    TObjectPtr<UCSStageSelectWidget> ActiveWidget;

    bool bUIOpen = false;            // server: is the select UI currently requested?
    bool bTransitionStarted = false; // server: has a choice been accepted?
    FString PendingTravelURL;        // server: resolved destination for DoServerTravel
};
