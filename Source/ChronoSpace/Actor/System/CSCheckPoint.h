// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CSCheckPoint.generated.h"

UENUM(BlueprintType)
enum class ECheckPointState : uint8
{
	Locked,
	Unlocked,
	Active
};

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* CheckPointMesh;

    // === Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckPoint Settings")
    int32 ChapterNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckPoint Settings")
    int32 StageNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckPoint Settings")
    int32 CheckPointNumber = 1;

    UPROPERTY(BlueprintReadOnly, Category = "CheckPoint State")
    FString CheckPointID;

    UPROPERTY(BlueprintReadOnly, Category = "CheckPoint State")
    ECheckPointState CurrentState = ECheckPointState::Locked;

public:
    // === Core Functions ===
    UFUNCTION(BlueprintCallable, Category = "CheckPoint")
    void ActivateCheckPoint();

    UFUNCTION(BlueprintCallable, Category = "CheckPoint")
    bool IsActive() const { return CurrentState == ECheckPointState::Active; }

    UFUNCTION(BlueprintCallable, Category = "CheckPoint")
    FString GetCheckPointID() const { return CheckPointID; }

    // === Events ===
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // === Blueprint Events ===
    UFUNCTION(BlueprintImplementableEvent, Category = "CheckPoint Events")
    void OnCheckPointActivated();

private:
    void GenerateID();
    class ACSGameMode* GetCSGameMode() const;
};
