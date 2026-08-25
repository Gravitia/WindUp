// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSSplitScreenTrigger.generated.h"

class UBoxComponent;

/**
 * 레벨에 배치하여 캐릭터가 진입하면 Split Screen → Full Screen 전환,
 * 벗어나면 Full Screen → Split Screen 전환을 수행하는 트리거 액터.
 */
UCLASS()
class CHRONOSPACE_API ACSSplitScreenTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACSSplitScreenTrigger();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** true이면 진입한 캐릭터의 화면만 풀스크린으로 전환. false이면 항상 Player 0 기준. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
	bool bFullScreenForEnteringPlayer = true;

	/** bFullScreenForEnteringPlayer가 false일 때 사용할 고정 플레이어 인덱스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen", meta = (EditCondition = "!bFullScreenForEnteringPlayer"))
	int32 FixedFullScreenPlayerIndex = 0;

private:
	/** 트리거 안에 있는 플레이어 수 */
	// 이 머신의 화면을 풀스크린으로 만든 캐릭터들 (로컬 판정 - 머신마다 다르다)
	TSet< TWeakObjectPtr<class ACharacter> > LocalTriggerCharacters;
};
