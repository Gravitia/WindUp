// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CSMimicWorldSubsystem.generated.h"

/**
 * 미믹 존(파란/빨간 구역)을 LinkChannel 이름으로 연결해주는 서브시스템.
 * 빨간 구역(CSMimicTargetZone)이 스스로 등록하고,
 * 파란 구역(CSMimicSourceZone)이 채널로 조회한다.
 */
UCLASS()
class CHRONOSPACE_API UCSMimicWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterTargetZone(class ACSMimicTargetZone* TargetZone);
	void UnregisterTargetZone(class ACSMimicTargetZone* TargetZone);

	// 채널에 등록된 유효한 빨간 구역 목록
	TArray<class ACSMimicTargetZone*> GetTargetZones(FName LinkChannel) const;

private:
	TMap<FName, TArray<TWeakObjectPtr<class ACSMimicTargetZone>>> TargetZonesByChannel;
};
