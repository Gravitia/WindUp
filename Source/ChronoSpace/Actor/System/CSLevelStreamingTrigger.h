// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Components/ShapeComponent.h"
#include "CSLevelStreamingTrigger.generated.h"

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSLevelStreamingTrigger : public ATriggerBox
{
	GENERATED_BODY()

public:
    ACSLevelStreamingTrigger();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
    int32 ChapterNumber;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Info")
    int32 StageNumber;

    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor,
        class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    bool IsServerPlayer(AActor* Actor);
    class UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;

};
