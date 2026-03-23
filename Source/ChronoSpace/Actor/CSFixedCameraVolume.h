// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSFixedCameraVolume.generated.h"

class UBoxComponent;
class UCameraComponent;

/**
 * 레벨에 배치하여 캐릭터가 진입하면 고정 카메라 시점으로 전환,
 * 벗어나면 원래 3인칭 카메라로 복원하는 트리거 액터.
 * 카메라는 이 액터의 CameraComponent 위치/회전에 고정됨.
 * ControlRotation은 Lerp로 부드럽게 전환되어 이동 방향이 자연스럽게 바뀜.
 */
UCLASS()
class CHRONOSPACE_API ACSFixedCameraVolume : public AActor
{
	GENERATED_BODY()

public:
	ACSFixedCameraVolume();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** 고정 카메라 (이 컴포넌트의 위치/회전이 카메라 시점이 됨) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Volume")
	TObjectPtr<UCameraComponent> FixedCamera;

	/** 카메라 전환 블렌드 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float BlendTime = 0.75f;

	/** 볼륨 내에서 사용할 고정 컨트롤러 회전 (이동 방향 기준) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	FRotator FixedControlRotation = FRotator(0.f, 0.f, 0.f);

	/** ControlRotation Lerp 전환 시간 (초). 카메라 BlendTime과 별도로 조정 가능 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float ControlRotationBlendTime = 0.75f;

	/** true이면 진입 시 풀스크린, 퇴장 시 스플릿 스크린 전환 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	bool bUseSplitScreenTransition = false;

	/** true이면 진입한 캐릭터의 화면만 풀스크린으로 전환. false이면 항상 FixedFullScreenPlayerIndex 기준 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition"))
	bool bFullScreenForEnteringPlayer = true;

	/** bFullScreenForEnteringPlayer가 false일 때 사용할 고정 플레이어 인덱스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume", meta = (EditCondition = "bUseSplitScreenTransition && !bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/** 진입 전 플레이어별 원래 컨트롤러 회전 저장 */
	TMap<APlayerController*, FRotator> SavedControlRotations;

	/** 트리거 안에 있는 플레이어 수 (스플릿 스크린 복원 판단용) */
	int32 PlayersInTrigger = 0;

	/** ControlRotation Lerp 상태 */
	struct FControlRotationLerpState
	{
		FRotator StartRotation;
		FRotator TargetRotation;
		float Elapsed = 0.f;
		float Duration = 0.75f;
		bool bIsLerping = false;
	};
	TMap<APlayerController*, FControlRotationLerpState> ControlRotationLerpStates;
};
