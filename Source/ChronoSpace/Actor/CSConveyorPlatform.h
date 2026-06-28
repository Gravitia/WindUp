// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSConveyorPlatform.generated.h"

class UStaticMeshComponent;
class ACSConveyorManager;

UCLASS()
class CHRONOSPACE_API ACSConveyorPlatform : public AActor
{
	GENERATED_BODY()
	
public:
	ACSConveyorPlatform();

	virtual void Tick(float DeltaSeconds) override;

	void SetManager(ACSConveyorManager* InManager);
	void SetIndexOffset(float InOffsetDistance);

	float GetOffsetDistance() const { return OffsetDistance; }

private:
	// wrap(끝 -> 시작 순간이동) 시 이 플랫폼 위에 올라탄 캐릭터가
	// 같이 텔레포트되지 않도록 movement base를 끊어준다.
	void DetachRidersOnWrap();

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Conveyor|Offset")
	float ZOffset = 0.f;

private:
	UPROPERTY()
	ACSConveyorManager* Manager = nullptr;

	// Manager�� (index * spacing)���� ����
	float OffsetDistance = 0.f;

	// wrap(순간이동) 감지를 위해 직전 프레임의 D(진행 거리) 값을 기억한다.
	float PrevD = 0.f;
	bool bHasPrevD = false;

};
