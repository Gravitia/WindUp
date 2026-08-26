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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSEditable|CheckPoint")
    class ACSRespawnPoint* ConnectedRespawnPoint;

public:
    // === Events ===
    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    /**
     * 폰이 이 체크포인트의 것이 맞으면 개인 리스폰 지점으로 설정한다.
     * 겹침 이벤트와 직접 조회가 같은 경로를 타도록 한 곳으로 모아 둔다.
     */
    bool TryClaimPawn(APawn* Pawn);

    /**
     * 폰이 서 있는 체크포인트를 도형으로 직접 찾아 적용한다.
     *
     * 겹침 이벤트에 기대면 레벨 전환 직후를 놓친다. 스폰 시점의 폰은 아직 빙의 전이라
     * PlayerState 가 없고, 그래서 IsPlayerControlled() 가 false 라 BeginOverlap 이 와도
     * 버려진다. 그 뒤로는 이미 겹친 상태라 새 BeginOverlap 도 오지 않는다.
     * 그래서 빙의가 끝난 뒤 이걸 한 번 호출해 준다.
     */
    static bool ClaimCheckPointAtPawnLocation(APawn* Pawn);

    // === Debug Functions ===
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugNetworkInfo() const;
};
