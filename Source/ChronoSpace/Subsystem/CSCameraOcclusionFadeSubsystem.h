// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CSCameraOcclusionFadeSubsystem.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/** 페이드 중인 액터 하나의 상태 */
USTRUCT()
struct FCSCameraOcclusionFadeEntry
{
	GENERATED_BODY()

	/** 값을 넣은 메시들. 복원 때 같은 메시에 0 을 넣는다. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Meshes;

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
 *
 * 매 틱 이 머신의 모든 뷰(로컬 플레이어 카메라 → 조작 캐릭터, 스플릿 보조 뷰 카메라 → 원격 캐릭터)에 대해
 * 카메라에서 캐릭터 캡슐 위 격자점들로 광선을 쏴 "막힌 광선 비율" 을 구하고, 그 비율을 목표로
 * 시간 보간한 값을 메시의 Custom Primitive Data 에 넣는다. 조금 겹치면 조금, 다 가리면 많이 투명해진다.
 *
 * 실제로 투명하게 그리는 건 머티리얼이다. 대상 오브젝트의 Material 은 Masked 이고 OpacityMask 에 MF_CameraFade 가
 * 꽂혀 있어야 한다. 그 함수가 이 값을 읽어 DitherTemporalAA 마스크를 만든다. 함수가 없는 머티리얼은 값을 넣어도
 * 아무 변화가 없다. 등록 시점에 이를 검사해 경고한다 (IsMaterialFadeReady).
 *
 * 순수 로컬 연출이다. 값은 복제되지 않고 각 머신이 자기 화면 기준으로 판정한다.
 */
UCLASS()
class CHRONOSPACE_API UCSCameraOcclusionFadeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
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

	/** 액터의 보이는 스태틱 메시를 모아 엔트리를 만든다 */
	void BeginFade(AActor* Actor);
	void ApplyFadeAmount(FCSCameraOcclusionFadeEntry& Entry, float Amount);
	void RestoreEntry(const FCSCameraOcclusionFadeEntry& Entry);
	void RestoreAll();

	/**
	 * 대상 액터의 머티리얼이 Masked 이고 페이드 함수를 참조하는지 검사하고, 아니면 머티리얼당 한 번 경고한다.
	 * 머티리얼의 캐시된 함수 목록(FMaterialCachedExpressionData::FunctionInfos)을 보므로 패키지 빌드에서도 동작한다.
	 */
	void ValidateActorMaterials(AActor* Actor);
	static bool IsMaterialFadeReady(const UMaterialInterface* Material);
	TSet<TWeakObjectPtr<const UMaterialInterface>> WarnedMaterials;

	/** 페이드 판정 후보. RegisterTarget 으로 들어온 액터들. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Targets;

	/** 페이드 중인 액터들. 키가 파괴되면 GC 가 null 로 만들므로 매 틱 정리한다. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<AActor>, FCSCameraOcclusionFadeEntry> Faded;
};
