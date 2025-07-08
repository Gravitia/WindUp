// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Components/ShapeComponent.h"
#include "CSCharacterTransitionTrigger.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSCharacterTransitionTrigger : public ATriggerBox
{
	GENERATED_BODY()
	
public:
    ACSCharacterTransitionTrigger();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    bool bCompleteStageOnTransition;

    UPROPERTY()
    TArray<class ACharacter*> PlayersInTrigger;

    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor,
        class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor,
        class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


    UFUNCTION(NetMulticast, Reliable)
    void MoveCharactersMulticast();

private:
    void CheckAllPlayersInTrigger();
    void MoveCharacterToPosition(class ACharacter* Character);

    class UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;
    class UCSGameProgressSubsystem* GetProgressSubsystem() const;
};
