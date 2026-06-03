// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSStagePortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACharacter;
class UCSGameProgressSubsystem;

/**
 * Co-op stage exit portal.
 * Waits for RequiredPlayerCount players to enter the trigger,
 * then fades out and ServerTravels to TargetLevelName.
 * Optionally marks the current stage cleared and updates last-played.
 *
 * For branching rest stages (one map → multiple destinations),
 * place several ACSStagePortal instances in the map, each with
 * a different TargetLevelName + NextChapter/NextStage.
 */
UCLASS()
class CHRONOSPACE_API ACSStagePortal : public AActor
{
    GENERATED_BODY()

public:
    ACSStagePortal();

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
    void MulticastStartTransition();

    void ApplyLightVisibility();
    void RecordProgressAndTravel();
    void PreloadTargetLevel();

    UCSGameProgressSubsystem* GetProgressSubsystem() const;

protected:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> Light1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> Light2;

    // === Travel ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Travel")
    FString TargetLevelName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Travel", meta = (ClampMin = "0.0"))
    float FadeDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Travel", meta = (ClampMin = "1", ClampMax = "4"))
    int32 RequiredPlayerCount = 2;

    // === Stage / Save ===
    // If true, marks (CurrentChapter, CurrentStage) cleared before travel.
    // Set false for rest/hub stages that shouldn't count toward progression.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage")
    bool bMarkCurrentStageCleared = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage",
        meta = (EditCondition = "bMarkCurrentStageCleared", ClampMin = "1"))
    int32 CurrentChapter = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage",
        meta = (EditCondition = "bMarkCurrentStageCleared", ClampMin = "1"))
    int32 CurrentStage = 1;

    // If true, sets last-played to (NextChapter, NextStage) before travel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage")
    bool bUpdateLastPlayed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage",
        meta = (EditCondition = "bUpdateLastPlayed", ClampMin = "1"))
    int32 NextChapter = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Stage",
        meta = (EditCondition = "bUpdateLastPlayed", ClampMin = "1"))
    int32 NextStage = 2;

    // === Runtime State ===
    UPROPERTY(ReplicatedUsing = OnRep_PlayerCount)
    int32 PlayerCount = 0;

    UPROPERTY()
    TArray<TObjectPtr<ACharacter>> PlayersInTrigger;

    bool bTransitionStarted = false;
    bool bPreloadStarted = false;
};
