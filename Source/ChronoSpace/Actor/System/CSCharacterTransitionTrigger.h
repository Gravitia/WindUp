// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Components/ShapeComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "CSCharacterTransitionTrigger.generated.h"

// Forward declarations
class UCSLevelStreamingSubsystem;
class UCSGameProgressSubsystem;
struct FStageData;

/**
 * 
 */
UCLASS()
class CHRONOSPACE_API ACSCharacterTransitionTrigger : public ATriggerBox
{
	GENERATED_BODY()
	
public:
    ACSCharacterTransitionTrigger();

    // 서버에서 레벨 스트리밍 시작 요청
    UFUNCTION(Server, Reliable)
    void ServerStartLevelStreaming();

    // 서버→모두(서버+클라이언트) 레벨 스트리밍 시작 RPC
    UFUNCTION(NetMulticast, Reliable)
    void MulticastStartLevelStreaming(int32 ChapterNumber, int32 StageNumber);

    // 클라이언트에 스트리밍 완료 알림
    UFUNCTION(Client, Reliable)
    void ClientOnLevelStreamingCompleted(int32 ChapterNumber, int32 StageNumber,
        FVector WorldSpawn, FVector CharacterSpawn);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

    // 오버랩 이벤트
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // 조건 검사
    void CheckTriggerConditions();

    // 실제 스트리밍 로직 (클라이언트·서버 공통으로 호출)
    bool PerformLevelStreaming(int32 ChapterNumber, int32 StageNumber);

    // 비동기 로딩 완료 콜백
    UFUNCTION()
    void OnAsyncLevelLoaded();

    // 캐릭터 이동
    UFUNCTION(Server, Reliable)
    void ServerMoveCharacters();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastMoveCharactersToNewPosition();

    // 상태 업데이트 RPC
    UFUNCTION(NetMulticast, Reliable)
    void MulticastUpdateTriggerState(int32 PlayerCount, bool bStreamingStarted, bool bMoveCompleted);

    void MoveLocalCharacterToNewPosition();

    // Getter/Setter
public:
    void SetNextStage(int32 ChapterNumber, int32 StageNumber);
    void SetRequiredPlayerCount(int32 Count);

    bool AreAllPlayersReady() const;
    int32 GetCurrentPlayerCount() const;
    bool IsLevelStreamingStarted() const;
    bool IsCharacterMoveCompleted() const;
    FVector GetCurrentSpawnPosition() const;
    FVector GetCurrentCharacterSpawnPosition() const;
    void GetCurrentStage(int32& OutChapter, int32& OutStage) const;

protected:
    UCSLevelStreamingSubsystem* GetLevelStreamingSubsystem() const;
    UCSGameProgressSubsystem* GetProgressSubsystem() const;

private:
    // 설정값
    UPROPERTY(EditAnywhere, Category = "Transition")
    bool bCompleteStageOnTransition;

    UPROPERTY(EditAnywhere, Category = "Transition")
    int32 RequiredPlayerCount;

    UPROPERTY(EditAnywhere, Category = "Transition")
    float TransitionDelay;

    UPROPERTY(Replicated)
    int32 NextChapterNumber;

    UPROPERTY(Replicated)
    int32 NextStageNumber;

    // 런타임 상태
    UPROPERTY(Replicated)
    bool bLevelStreamingStarted;

    UPROPERTY(Replicated)
    bool bCharacterMoveCompleted;

    UPROPERTY()
    TArray<ACharacter*> PlayersInTrigger;

    // 레벨 스트리밍 관련
    UPROPERTY()
    ULevelStreamingDynamic* CurrentStreamingLevel;

    bool bIsAsyncStreaming;
    int32 PendingChapterNumber;
    int32 PendingStageNumber;
    FStageData* PendingStageData;

    // 완료된 스테이지 위치
    UPROPERTY(Replicated)
    int32 CurrentChapter;
    UPROPERTY(Replicated)
    int32 CurrentStage;
    UPROPERTY(Replicated)
    FVector CurrentSpawnPosition;
    UPROPERTY(Replicated)
    FVector CurrentCharacterSpawnPosition;
};
