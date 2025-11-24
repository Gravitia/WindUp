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
    UPROPERTY(EditAnywhere)
    UStaticMeshComponent* Mesh;

    // --------------------------
    // Linked Actor (push target)
    // --------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    TObjectPtr<AActor> LinkedActor;

    // Force applied (PressTwo only)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Object")
    FVector LinkedForce = FVector(0.f, 0.f, -30000.f);

    bool bLinkedActorPushed = false;


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

    // scale values
    UPROPERTY(EditAnywhere)
    FVector IdleScale = FVector(1, 1, 1);

    UPROPERTY(EditAnywhere)
    FVector PressOneScale = FVector(1, 1, 0.7f);

    UPROPERTY(EditAnywhere)
    FVector PressTwoScale = FVector(1, 1, 0.45f);


    // --------------------------
    // Push Linked Actor (Force)
    // --------------------------
    void PushLinkedActor();


    // --------------------------
    // Landing Event From Character
    // --------------------------
    UFUNCTION()
    void NotifyPlayerLanded(ACharacter* PlayerCharacter); // from Character::Landed()


    // auto reset timer
    FTimerHandle TimerHandle_Reset;


    // replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
