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
	// ConveyorManager에서 생성 직후 호출
	void Init(ACSConveyorManager* InManager, float InOffsetDistance);
	float GetMeshLength() const;

protected:
	virtual void BeginPlay() override;

private:
	// 실제 바닥 역할을 하는 메쉬
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	// 부모 컨베이어 매니저 (단일 진실 소스)
	UPROPERTY()
	ACSConveyorManager* Manager = nullptr;

	// 스플라인 기준 오프셋
	float OffsetDistance = 0.f;

	// 높이 보정
	UPROPERTY(EditAnywhere, Category = "Conveyor")
	float ZOffset = 0.f;

};
