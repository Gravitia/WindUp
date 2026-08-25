// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSCameraZoomVolume.generated.h"

class UBoxComponent;

/**
 * 레벨에 배치하여 캐릭터가 진입하면 SpringArm 거리를 줄여 카메라를 가깝게 전환하고,
 * 벗어나면 원래 거리로 복원하는 트리거 액터.
 */
UCLASS()
class CHRONOSPACE_API ACSCameraZoomVolume : public AActor
{
	GENERATED_BODY()

public:
	ACSCameraZoomVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default|Camera Volume")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** 줄일 카메라 암 거리 (양수: 줌인, 예: 400이면 기본 거리보다 400 가깝게) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float ZoomLength = 400.f;

	/** 줌 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default|Camera Volume")
	float ZoomSpeed = 5.f;

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
	/** 트리거 안에 있는 플레이어 수 (스플릿 스크린 복원 판단용) */
	/** 볼륨 안 캐릭터 1명분 상태 (겹침 수를 세어 Begin/End 중복을 흡수) */
	struct FCSZoomOccupant
	{
		int32 OverlapCount = 0;
		bool bRequestedFullScreen = false;
	};

	TMap< TWeakObjectPtr<class ACharacter>, FCSZoomOccupant > Occupants;
};
