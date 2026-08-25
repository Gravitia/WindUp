// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSBellows.generated.h"

UENUM(BlueprintType)
enum class EBellowsState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    PressOnePlayer  UMETA(DisplayName = "Press One Player"),
    PressTwoPlayer  UMETA(DisplayName = "Press Two Player")
};

class UStaticMeshComponent;
class UBoxComponent;
class ACharacter;

UCLASS()
class CHRONOSPACE_API ACSBellows : public AActor
{
    GENERATED_BODY()

public:
    ACSBellows();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

public:
    // --------------------------
    // Components
    // --------------------------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bellows")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bellows")
    UBoxComponent* PressTrigger;

    // --------------------------
    // Linked Actor (LERP target)
    // --------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    TObjectPtr<AActor> LinkedActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    float LinkedMoveDistance = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    float LinkedMoveDuration = 0.25f;

protected:
    bool bLinkedLerping = false;

    // 늦게 접속한 클라도 "연결된 액터가 이미 옮겨졌는지"를 알아야 한다.
    // 예전엔 비복제라 BellowsState 가 Idle 로 돌아온 뒤 접속하면 문이 닫힌 자리에 그대로 보였다.
    UPROPERTY(ReplicatedUsing = OnRep_LinkedMoved)
    bool bLinkedMoved = false;

    UFUNCTION()
    void OnRep_LinkedMoved();

    FVector LinkedStartLoc = FVector::ZeroVector;
    FVector LinkedTargetLoc = FVector::ZeroVector;
    float LinkedLerpAlpha = 0.f;

    void StartLinkedActorLerp();
    void TickLinkedActorLerp(float DeltaTime);

    // --------------------------
    // Bellows State
    // --------------------------
    UPROPERTY(ReplicatedUsing = OnRep_BellowsState, BlueprintReadOnly, Category = "Bellows")
    EBellowsState BellowsState = EBellowsState::Idle;

    UFUNCTION()
    void OnRep_BellowsState();

    void ApplyState(EBellowsState NewState);
    void RecomputeStateFromOverlap();

    // --------------------------
    // Scale LERP
    // --------------------------
    bool bScaleLerping = false;
    FVector StartScale = FVector(1.f, 1.f, 1.f);
    FVector TargetScale = FVector(1.f, 1.f, 1.f);
    float ScaleLerpAlpha = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
    float ScaleLerpDuration = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
    FVector IdleScale = FVector(1.f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
    FVector PressOneScale = FVector(1.f, 1.f, 0.7f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
    FVector PressTwoScale = FVector(1.f, 1.f, 0.45f);

    void StartScaleLerp(const FVector& NewTargetScale);

    // --------------------------
    // PressTwo Hold
    // --------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bellows")
    float PressTwoHoldTime = 3.0f;

    bool bHoldPressTwoState = false;

    FTimerHandle TimerHandle_PressTwoHold;

    void StartPressTwoHold();
    void EndPressTwoHold();

    // --------------------------
    // Trigger overlap tracking
    // --------------------------
    UPROPERTY()
    TSet<TWeakObjectPtr<ACharacter>> OverlappingPlayers;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void OnTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};