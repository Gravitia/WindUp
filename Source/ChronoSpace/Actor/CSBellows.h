// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CSBellows.generated.h"

UENUM(BlueprintType)
enum class EBellowsState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	PressOnePlayer  UMETA(DisplayName = "Press One Player"),
	PressTwoPlayer  UMETA(DisplayName = "Press Two Player")
};

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
    // Bellows Mesh
    // --------------------------
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Mesh;

    UPROPERTY()
    TWeakObjectPtr<ACharacter> FirstPressedPlayer;


    // --------------------------
    // Linked Actor (LERP target)
    // --------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    TObjectPtr<AActor> LinkedActor;

    // Y 방향으로 이동할 값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    float LinkedMoveDistance = 1000.f;

    // Lerp duration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    float LinkedMoveDuration = 0.25f;

    // Lerp control
    bool bLinkedLerping = false;
    bool bLinkedMoved = false;

    FVector LinkedStartLoc;
    FVector LinkedTargetLoc;
    float LinkedLerpAlpha = 0.f;

    void StartLinkedActorLerp();
    void TickLinkedActorLerp(float DeltaTime);


    // --------------------------
    // Bellows State
    // --------------------------
    UPROPERTY(ReplicatedUsing = OnRep_BellowsState)
    EBellowsState BellowsState = EBellowsState::Idle;

    UFUNCTION()
    void OnRep_BellowsState();

    void ChangeState(EBellowsState NewState);
    void ResetToIdle();


    // --------------------------
    // Scale LERP
    // --------------------------
    bool bScaleLerping = false;
    FVector StartScale;
    FVector TargetScale;
    float ScaleLerpAlpha = 0.f;

    UPROPERTY(EditAnywhere, Category = "Scale")
    float ScaleLerpDuration = 0.15f;

    void StartScaleLerp(const FVector& NewTargetScale);

    UPROPERTY(EditAnywhere)
    FVector IdleScale = FVector(1, 1, 1);

    UPROPERTY(EditAnywhere)
    FVector PressOneScale = FVector(1, 1, 0.7f);

    UPROPERTY(EditAnywhere)
    FVector PressTwoScale = FVector(1, 1, 0.45f);


    // --------------------------
    // Landing Event (From Character::Landed)
    // --------------------------
    UFUNCTION()
    void NotifyPlayerLanded(ACharacter* PlayerCharacter);


    // auto reset bellows scale
    FTimerHandle TimerHandle_Reset;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
