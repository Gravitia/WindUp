// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSMimicSourceZone.generated.h"

/**
 * 파란 구역 - 플레이어가 들어오면 같은 LinkChannel의 모든 빨간 구역(CSMimicTargetZone)에
 * 그 플레이어의 행동(이동/점프/능력/스케일)을 따라하는 분신을 생성한다.
 * 플레이어가 구역을 나가면 분신은 소멸한다.
 * 파란 구역은 한 번에 한 명만 점유할 수 있고, 서로 다른 파란 구역에 두 플레이어가 각각 들어갈 수 있다.
 */
UCLASS()
class CHRONOSPACE_API ACSMimicSourceZone : public AActor
{
	GENERATED_BODY()

public:
	ACSMimicSourceZone();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 서버 전용: 플레이어가 구역을 점유 - 빨간 구역들에 분신 스폰 + 능력 입력 구독
	void Occupy(class ACSCharacterPlayer* Player);

	// 서버 전용: 점유 해제 - 분신 소멸 + 구독 해제
	void Release();

	void OnSourceGASInput(int32 InputId, bool bPressed);

protected:
	// 빨간 구역과 연결되는 채널 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mimic")
	FName LinkChannel = TEXT("Default");

	// 스폰할 분신 클래스 (플레이어 BP를 복제해 CSMimicCharacter로 리페어런팅한 BP를 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mimic")
	TSubclassOf<class ACSMimicCharacter> MimicCharacterClass;

	// 플레이어 진입 감지용 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UBoxComponent> ZoneBox;

	// 구역 표시 메시 (파란색 - 중력반전 머티리얼 재사용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UStaticMeshComponent> ZoneMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mimic")
	TObjectPtr<class UMaterial> ZoneMaterial;

	// 박스 크기에 메시 스케일/타일링을 맞춘다
	void UpdateZoneMesh();

	// 현재 이 구역을 점유 중인 플레이어 (서버 전용)
	TWeakObjectPtr<class ACSCharacterPlayer> Occupant;

	// 이 구역이 스폰한 분신들 (서버 전용)
	UPROPERTY()
	TArray<TObjectPtr<class ACSMimicCharacter>> SpawnedMimics;
};
