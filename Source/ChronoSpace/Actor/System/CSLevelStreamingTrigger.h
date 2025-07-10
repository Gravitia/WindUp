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

    // 스트리밍할 챕터 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Streaming")
    int32 ChapterNumber;

    // 스트리밍할 스테이지 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Streaming")
    int32 StageNumber;

    // 오버랩 이벤트 처리
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

private:
    // 유효한 플레이어인지 확인 (기존 IsServerPlayer에서 이름 변경)
    bool IsValidPlayer(AActor* Actor);

    // 레벨 스트리밍 서브시스템 가져오기
    class UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;

};
