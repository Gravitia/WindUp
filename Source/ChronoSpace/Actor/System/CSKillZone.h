// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CSKillZone.generated.h"

UENUM(BlueprintType)
enum class EKillZoneType : uint8
{
	Fall,
	Lava,
	Water,
	Toxic,
	Laser,
	Void
};

UCLASS()
class CHRONOSPACE_API ACSKillZone : public AActor
{
	GENERATED_BODY()
	
public:
    ACSKillZone();

protected:
    virtual void BeginPlay() override;

public:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* KillVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* VisualMesh;

    // === Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Zone Settings")
    EKillZoneType KillZoneType = EKillZoneType::Fall;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Zone Settings")
    bool bIsActive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kill Zone Settings")
    bool bAffectsPlayersOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual Settings")
    bool bShowVisualMesh = false;

public:
    // === Core Functions ===
    UFUNCTION(BlueprintCallable, Category = "Kill Zone")
    void KillPlayer(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Kill Zone")
    void SetActive(bool bNewActive);

    // === Events ===
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION(BlueprintCallable, Category = "Kill Zone")
    void KillPlayerWithDelay(APawn* Player, float DelayTime);

private:
    class ACSGameState* GetCSGameState() const;
    class ACSGameMode* GetCSGameMode() const;
};
