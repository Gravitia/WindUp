// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "CSCheckPoint.generated.h"


UCLASS()
class CHRONOSPACE_API ACSCheckPoint : public AActor
{
	GENERATED_BODY()
	
public:
    ACSCheckPoint();

protected:
    virtual void BeginPlay() override;

public:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // === Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckPoint")
    class ACSRespawnPoint* ConnectedRespawnPoint;

public:
    // === Events ===
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    class ACSGameMode* GetCSGameMode() const;
};
