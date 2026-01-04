// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSAutoTrap.generated.h"

UENUM(BlueprintType)
enum class ETrapStepSlot : uint8
{
    First   UMETA(DisplayName = "First"),
    Second  UMETA(DisplayName = "Second"),
    Third   UMETA(DisplayName = "Third"),
    Extra   UMETA(DisplayName = "Extra")
};

USTRUCT(BlueprintType)
struct FTrapStep
{
    GENERATED_BODY()

    // 이 스텝까지의 대기 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Delay = 1.0f;

    // 몇 번째 스텝인지
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ETrapStepSlot StepSlot = ETrapStepSlot::First;
};


UCLASS()
class CHRONOSPACE_API ACSAutoTrap : public AActor
{
	GENERATED_BODY()
	
public:
    ACSAutoTrap();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /* =========================
     * Pattern / Step System
     * ========================= */

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Pattern")
    TArray<FTrapStep> TrapSteps;

    int32 CurrentStepIndex = 0;

    void ExecuteCurrentStep();

    FTimerHandle TrapTimer;

    /* =========================
     * Replication
     * ========================= */

     // 현재 실행 중인 스텝 슬롯
    UPROPERTY(ReplicatedUsing = OnRep_CurrentStep)
    ETrapStepSlot CurrentStepSlot = ETrapStepSlot::First;

    UFUNCTION()
    void OnRep_CurrentStep();

    /* =========================
     * Blueprint Hooks
     * ========================= */

     // 눌림/해제 같은 상태 연출용
    UFUNCTION(BlueprintImplementableEvent)
    void PlayTrapAnim(bool bPressed);

    // 스텝 실행 이벤트 (행동 해석은 BP 책임)
    UFUNCTION(BlueprintImplementableEvent)
    void OnTrapStep(ETrapStepSlot StepSlot);
};
