// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSKillZoneResetComponent.generated.h"

class UBoxComponent;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSKillZoneResetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UCSKillZoneResetComponent();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Default|LevelDesign")
    FVector RespawnOffset = FVector(0.f, 0.f, 50.f);

private:
    UFUNCTION()
    void OnBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

private:
    UPROPERTY()
    TObjectPtr<UBoxComponent> OverlapBox = nullptr;

    FVector StartLocation = FVector::ZeroVector;

public:
    // KillZone Immortal
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|KillZone")
    bool bIgnoreKillZone = false;
		
};
