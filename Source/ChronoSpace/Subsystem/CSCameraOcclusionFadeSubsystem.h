// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CSCameraOcclusionFadeSubsystem.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FCSCameraFadeMaterialData;

USTRUCT()
struct FCSMaterialSlots
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;
};

/** 페이드 중인 액터 하나의 상태. 원본 머티리얼은 강한 참조로 들고 있어야 슬롯을 비운 MID 가 GC 로 사라지지 않는다. */
USTRUCT()
struct FCSCameraOcclusionFadeEntry
{
	GENERATED_BODY()

	/** 메시별 원본 머티리얼 (슬롯 순서) */
	UPROPERTY()
	TMap<TObjectPtr<UStaticMeshComponent>, FCSMaterialSlots> Originals;

	/** 메시별로 우리가 끼운 페이드 MID (슬롯 순서). 복원 시 이것과 같은 슬롯만 되돌린다. */
	UPROPERTY()
	TMap<TObjectPtr<UStaticMeshComponent>, FCSMaterialSlots> FadeMIDs;

	/** 이번 틱 판정된 목표값 (가려진 광선 비율 0..1). 안 걸리면 0. */
	float TargetFade = 0.f;

	/** 시간 보간 중인 현재값. 0 으로 돌아오면 엔트리를 정리한다. */
	float CurrentFade = 0.f;
};

/**
 * 카메라와 캐릭터 사이를 막는 오브젝트를 반투명으로 페이드한다.
 *
 * 대상은 RegisterTarget 으로 등록된 액터의 스태틱 메시다. 지금은 UCSMeshPulledByBlackhole 과
 * UCSMeshAffectedByGravityCore 가 BeginPlay/EndPlay 에서 등록·해제한다.
 * 이 메시들은 카메라 채널을 무시하므로 스프링암이 줄어들지 않는 대신 캐릭터를 가릴 수 있다.
 * (UCSManagedActorSubsystem 의 블랙홀 목록을 빌려 쓰지 않는다. 그 목록은 캐릭터·투사체 트레이스가
 *  무시할 액터 목록이라, 페이드 대상을 거기 섞으면 게임플레이가 바뀐다.)
 *
 * 매 틱 이 머신의 모든 뷰(로컬 플레이어 카메라 → 조작 캐릭터, 스플릿 보조 뷰 카메라 → 원격 캐릭터)에 대해
 * 카메라에서 캐릭터 캡슐 위 격자점들로 광선을 쏴 "막힌 광선 비율" 을 구하고, 그 비율을 목표로
 * 시간 보간한 값을 페이드 MID 의 FadeAmount 에 넣는다. 조금 겹치면 조금, 다 가리면 많이 투명해진다.
 *
 * 페이드 머티리얼은 하나(설정의 FadeMaterial, Masked + 디더)다. 원본 머티리얼에서 뽑아낸 텍스처·색
 * (FCSCameraFadeMaterials → FCSCameraFadeMaterialData)을 파라미터로 넣어 원본과 같은 색에서 투명해진다.
 * 뽑아낼 수 없는 머티리얼은 경고만 남기고 그 슬롯은 건드리지 않는다. 틀린 색으로 그리느니 안 하는 게 낫다.
 *
 * 순수 로컬 연출이다. 머티리얼 교체는 복제되지 않고 각 머신이 자기 화면 기준으로 판정한다.
 */
UCLASS()
class CHRONOSPACE_API UCSCameraOcclusionFadeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/** 이 액터의 스태틱 메시를 페이드 판정 대상에 넣는다 / 뺀다. 빼면 페이드 중이던 것도 즉시 복원한다. */
	void RegisterTarget(AActor* Actor);
	void UnregisterTarget(AActor* Actor);

private:
	struct FViewPair
	{
		FVector CameraLocation;
		AActor* Target;
	};

	/** 이 머신에서 그려지는 (카메라 위치, 비추는 캐릭터) 쌍을 모은다 */
	void CollectViews(TArray<FViewPair>& OutViews) const;

	/** 캐릭터 캡슐 위에 카메라 방향 기준 격자 샘플점을 만든다 */
	static void BuildSamplePoints(const FViewPair& View, TArray<FVector>& OutPoints);

	/** 한 뷰에 대해 후보 액터마다 막힌 광선 비율을 구해 목표 페이드를 갱신한다 */
	void UpdateTargets(const FViewPair& View, const TArray<TObjectPtr<AActor>>& Candidates);

	/**
	 * 액터를 페이드 상태로 만든다 (MID 생성, 슬롯 교체).
	 * 전부 아니면 안 함: 보이는 메시의 슬롯 하나라도 추출 데이터가 없으면 액터를 건드리지 않고 Faded 에 넣지 않는다.
	 * 몸통은 불투명한데 버튼만 투명해지는 반쪽 고스트가 그냥 불투명한 것보다 더 이상하다.
	 */
	void BeginFade(AActor* Actor);
	void ApplyFadeAmount(FCSCameraOcclusionFadeEntry& Entry, float Amount);
	void RestoreEntry(const FCSCameraOcclusionFadeEntry& Entry);
	void RestoreAll();

	/** 원본에서 뽑아낸 페이드 데이터. 없으면 nullptr 이고 한 번만 경고한다. */
	const FCSCameraFadeMaterialData* ResolveFadeData(UMaterialInterface* Original);

	/** 뽑아낼 수 없어 경고를 이미 남긴 원본들. 매 틱 같은 경고를 반복하지 않기 위해서다. */
	TSet<TWeakObjectPtr<UMaterialInterface>> WarnedUnsupported;
	/** 슬롯 하나가 안 돼 통째로 건너뛴다고 이미 경고한 액터들 */
	TSet<TWeakObjectPtr<AActor>> WarnedActors;
	bool bWarnedNoFadeMaterial = false;

	/** 페이드 판정 후보. RegisterTarget 으로 들어온 액터들. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Targets;

	/** 페이드 중인 액터들. 키가 파괴되면 GC 가 null 로 만들므로 매 틱 정리한다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<AActor>, FCSCameraOcclusionFadeEntry> Faded;
};
