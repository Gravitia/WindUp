// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "CSConveyor.generated.h"


UCLASS()
class CHRONOSPACE_API ACSConveyor : public AActor
{
	GENERATED_BODY()
	
public:
    // Sets default values for this actor's properties
    ACSConveyor();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* Box;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* Direction;

    // Conveyor Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor", meta = (ClampMin = "0.0"))
    float Speed = 300.f; // cm/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor", meta = (ClampMin = "0.0"))
    float ForceScale = 500.f; // Æ©´× °ª

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conveyor")
    bool bAffectCharacters = true;

    // Runtime data
    TSet<TWeakObjectPtr<UPrimitiveComponent>> OverlappingPrims;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

protected:
    // Overlap Events
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    // Helper functions
    void ApplyForceToPhysicsObject(UPrimitiveComponent* Prim, const FVector& BeltDirection, float DeltaTime);
    void ApplyForceToCharacter(ACharacter* Character, const FVector& BeltDirection, float DeltaTime);

};
