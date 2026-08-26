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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|AutoTrap|Step")
    float Delay = 1.0f;

    // 몇 번째 스텝인지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|AutoTrap|Step")
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CSEditable|AutoTrap")
    TArray<FTrapStep> TrapSteps;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Default")
    int32 CurrentStepIndex = 0;

    void ExecuteCurrentStep();

    FTimerHandle TrapTimer;

    /* =========================
     * Replication
     * ========================= */

     // 현재 스텝 슬롯 (값 자체는 복제되지만, 변경이 없으면 OnRep는 안 뜰 수 있음)
    UPROPERTY(Replicated)
    ETrapStepSlot CurrentStepSlot = ETrapStepSlot::First;

    // "스텝이 실행됐다"를 보장하는 시리얼 (매 스텝마다 무조건 증가 -> OnRep가 항상 호출)
    UPROPERTY(ReplicatedUsing = OnRep_StepSerial)
    uint8 StepSerial = 0;

    UFUNCTION()
    void OnRep_StepSerial();

    /* =========================
     * Blueprint Hooks
     * ========================= */

     // 눌림/해제 같은 상태 연출용(필요하면 BP에서 사용)
    UFUNCTION(BlueprintImplementableEvent)
    void PlayTrapAnim(bool bPressed);

    // 스텝 실행 이벤트 (행동 해석은 BP 책임)
    UFUNCTION(BlueprintImplementableEvent)
    void OnTrapStep(ETrapStepSlot StepSlot);

private:
    // 디버그/안전용: 서버에서만 스텝 진행을 시작하도록 강제
    void StartServerPattern();
};
