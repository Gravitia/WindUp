// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSButtonIndicatorComponent.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * CSMeshAffectedByGravityCore / CSMeshPulledByBlackhole 와 같은 액터에 붙이는 컴포넌트.
 * 형제 Gravity 컴포넌트를 자동 탐색해 Delegate에 바인딩하고,
 * 상호작용 시작/종료 시 ButtonMeshes에 지정된 StaticMeshComponent의 머티리얼을 전환한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHRONOSPACE_API UCSButtonIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSButtonIndicatorComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnInteractionStarted();

	UFUNCTION()
	void OnInteractionEnded();

protected:
	/**
	 * 머티리얼을 전환할 버튼 StaticMeshComponent 목록.
	 * 비워두면 오너의 StaticMeshComponent 중 이름에 "button"이 포함된 것을 자동 탐색.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button Indicator")
	TArray<TObjectPtr<UStaticMeshComponent>> ButtonMeshes;

	/** 활성화 시 적용할 머티리얼 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button Indicator")
	TObjectPtr<UMaterialInterface> ActivatedMaterial;

	/** 머티리얼을 교체할 슬롯 인덱스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button Indicator")
	int32 MaterialSlotIndex = 0;

private:
	struct FButtonMeshCache
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UMaterialInterface> OriginalMaterial;
	};
	TArray<FButtonMeshCache> CachedButtonMeshes;

	void BindToSiblingComponents();
	void UnbindFromSiblingComponents();
	void CacheButtonMeshes();
	void SetButtonMaterials(UMaterialInterface* Material);
};
