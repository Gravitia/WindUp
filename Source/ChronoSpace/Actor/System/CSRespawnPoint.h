// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ArrowComponent.h"
#include "CSRespawnPoint.generated.h"


UCLASS()
class CHRONOSPACE_API ACSRespawnPoint : public AActor
{
	GENERATED_BODY()
	
public:
    ACSRespawnPoint();

public:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* DirectionArrow;

public:
    // === Core Functions ===
    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    void SpawnPlayerHere(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    FVector GetRespawnLocation() const { return GetActorLocation(); }

    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    FRotator GetRespawnRotation() const { return GetActorRotation(); }
};
