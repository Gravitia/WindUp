// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSRotatingActor.generated.h"

UCLASS()
class CHRONOSPACE_API ACSRotatingActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACSRotatingActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Static Mesh 컴포넌트 (블루프린트에서 설정 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Mesh")
	UStaticMeshComponent* MeshComponent;

	// 회전 속도 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float RotationSpeed = 360.0f; // 초당 360도 회전

private:
	// 서버 시각 기준 결정론 회전용 (모든 머신이 같은 각도를 계산 - 복제 트래픽 0)
	FRotator InitialRotation = FRotator::ZeroRotator;

	/** GameState 의 서버 동기화 시각. GameState 가 아직 없으면 로컬 시간으로 폴백. */
	float GetSynchronizedWorldTime() const;
};
