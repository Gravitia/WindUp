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

    // === 전환 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    bool bCompleteStageOnTransition;

    // 필요한 플레이어 수 (기본 2명)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "1", ClampMax = "4"))
    int32 RequiredPlayerCount;

    // 다음 챕터 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    int32 NextChapterNumber;

    // 다음 스테이지 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    int32 NextStageNumber;

    // 전환 대기 시간 (레벨 로딩 후 캐릭터 이동까지의 시간)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float TransitionDelay;

    // 현재 트리거 안에 있는 플레이어들 (Replicated)
    UPROPERTY(BlueprintReadOnly, Category = "Transition", Replicated)
    TArray<class ACharacter*> PlayersInTrigger;

    // 레벨 스트리밍이 시작되었는지 여부 (Replicated)
    UPROPERTY(BlueprintReadOnly, Category = "Transition", Replicated)
    bool bLevelStreamingStarted;

    // 캐릭터 이동이 완료되었는지 여부 (Replicated)
    UPROPERTY(BlueprintReadOnly, Category = "Transition", Replicated)
    bool bCharacterMoveCompleted;

    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor,
        class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor,
        class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // === Multicast RPC Functions ===

    // 모든 클라이언트에서 레벨 전환 실행
    UFUNCTION(NetMulticast, Reliable)
    void MulticastTransitionToNextStage();

    // 모든 클라이언트에서 캐릭터 이동 실행
    UFUNCTION(NetMulticast, Reliable)
    void MulticastMoveCharactersToNewPosition();

    // 트리거 상태 업데이트 (모든 클라이언트 동기화)
    UFUNCTION(NetMulticast, Reliable)
    void MulticastUpdateTriggerState(int32 PlayerCount, bool bStreamingStarted, bool bMoveCompleted);

private:
    void CheckTriggerConditions();

    // 서버에서만 실행되는 레벨 스트리밍 로직
    void ServerStartLevelStreaming();

    // 서버에서만 실행되는 캐릭터 이동 로직
    void ServerMoveCharacters();

    // 각 클라이언트에서 실행되는 캐릭터 이동
    UFUNCTION()
    void MoveLocalCharacterToNewPosition();

    // 타이머 핸들
    FTimerHandle TransitionTimerHandle;

    // Replication을 위한 함수
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // === 공개 함수 ===

    // 다음 스테이지 설정
    UFUNCTION(BlueprintCallable, Category = "Transition")
    void SetNextStage(int32 ChapterNumber, int32 StageNumber);

    // 필요한 플레이어 수 설정
    UFUNCTION(BlueprintCallable, Category = "Transition")
    void SetRequiredPlayerCount(int32 Count);

    // 현재 트리거 상태 확인
    UFUNCTION(BlueprintCallable, Category = "Transition")
    bool AreAllPlayersReady() const;

    // 현재 트리거 안의 플레이어 수
    UFUNCTION(BlueprintCallable, Category = "Transition")
    int32 GetCurrentPlayerCount() const;

    // 레벨 스트리밍 시작 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Transition")
    bool IsLevelStreamingStarted() const;

    // 캐릭터 이동 완료 여부 확인
    UFUNCTION(BlueprintCallable, Category = "Transition")
    bool IsCharacterMoveCompleted() const;

private:
    class UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;
    class UCSGameProgressSubsystem* GetProgressSubsystem() const;
};
