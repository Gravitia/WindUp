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
	// Sets default values for this actor's properties
	ACSCheckPoint();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UBoxComponent* TriggerBox;

    // Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ChapterNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StageNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CheckPointNumber = 1;

    UPROPERTY(BlueprintReadOnly)
    FString CheckPointID;

    UPROPERTY(BlueprintReadOnly)
    ECheckPointState CurrentState = ECheckPointState::Locked;

public:
    UFUNCTION(BlueprintCallable)
    void UnlockCheckPoint();

    UFUNCTION(BlueprintCallable)
    void ActivateCheckPoint();

    UFUNCTION(BlueprintCallable)
    bool IsActive() const { return CurrentState == ECheckPointState::Active; }

    UFUNCTION(BlueprintCallable)
    FString GetCheckPointID() const { return CheckPointID; }

    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    void GenerateID();
};
