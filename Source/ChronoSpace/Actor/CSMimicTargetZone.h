// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSMimicTargetZone.generated.h"

/**
 * 빨간 구역 - 같은 LinkChannel의 파란 구역(CSMimicSourceZone)에 플레이어가 들어오면
 * 이 구역에 플레이어를 따라하는 분신이 생성된다. 레벨에 여러 개 배치 가능.
 */
UCLASS()
class CHRONOSPACE_API ACSMimicTargetZone : public AActor
{
	GENERATED_BODY()

public:
	ACSMimicTargetZone();

	virtual void OnConstruction(const FTransform& Transform) override;

	FORCEINLINE FName GetLinkChannel() const { return LinkChannel; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 파란 구역과 연결되는 채널 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mimic")
	FName LinkChannel = TEXT("Default");

	// 구역 범위 표시용 박스 (게임플레이 충돌 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UBoxComponent> ZoneBox;

	// 구역 표시 메시 (빨간색 - 시간정지 머티리얼 재사용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UStaticMeshComponent> ZoneMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UMaterial> ZoneMaterial;

	// 박스 크기에 메시 스케일/타일링을 맞춘다
	void UpdateZoneMesh();
};
