// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ArrowComponent.h"
#include "CSRespawnPoint.generated.h"


UCLASS()
class CHRONOSPACE_API ACSRespawnPoint : public AActor
{
	GENERATED_BODY()
	
public:
    ACSRespawnPoint();

public:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|RespawnPoint")
    UArrowComponent* DirectionArrow;

public:

    UFUNCTION(BlueprintCallable, Category = "CS|RespawnPoint")
    FVector GetRespawnLocation() const { return GetActorLocation(); }

    UFUNCTION(BlueprintCallable, Category = "CS|RespawnPoint")
    FRotator GetRespawnRotation() const { return GetActorRotation(); }

    /**
     * 개인 리스폰 지점이 아직 없으면 폰 위치에서 가장 가까운 것으로 채워 준다.
     *
     * 레벨 전환 직후엔 PersonalRespawnPoint 가 null 로 시작하는데, 체크포인트 밖에서
     * 스폰되면 계속 null 이라 죽어도 RespawnSinglePlayer 가 그냥 실패한다. 그 구멍을 막는다.
     * 이미 잡혀 있으면 손대지 않는다 - 진행하며 갱신된 지점을 덮어쓰면 안 된다.
     *
     * @return 새로 채워 넣었으면 true
     */
    static bool EnsureRespawnPoint(APawn* Pawn);
};
