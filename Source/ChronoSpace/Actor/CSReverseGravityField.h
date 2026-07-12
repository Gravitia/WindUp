// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSReverseGravityField.generated.h"

/**
 * 앵커 아이템(CSGravityAnchorItem) 기반 중력 반전 필드.
 * 같은 GroupChannel의 아이템이 RequiredItemCount(기본 4)개 이상 배치되면,
 * 아이템들의 XY 범위를 감싸고 높이 FieldHeight(기본 1000)인 중력 반전 영역이 활성화된다.
 * 아이템을 옮기면 영역도 따라 움직이고, 개수가 모자라면 비활성화된다.
 */
UCLASS()
class CHRONOSPACE_API ACSReverseGravityField : public AActor
{
	GENERATED_BODY()

public:
	ACSReverseGravityField();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용: 앵커 아이템을 모아 필드 영역을 갱신
	void UpdateField();

	// 복제된 영역 상태를 컴포넌트에 적용 (서버/클라이언트 공통)
	void ApplyFieldState();

	UFUNCTION()
	void OnRep_FieldState();

	UFUNCTION()
	void OnFieldBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnFieldEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	// 앵커 아이템과 연결되는 채널 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	FName GroupChannel = TEXT("Default");

	// 필드 활성화에 필요한 아이템 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	int32 RequiredItemCount = 4;

	// 필드 높이 (아이템 최저점 기준 위로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	float FieldHeight = 1000.0f;

	// 영역 갱신 주기 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	float UpdateInterval = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	TObjectPtr<class UBoxComponent> FieldBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	TObjectPtr<class UStaticMeshComponent> FieldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Field")
	TObjectPtr<class UMaterial> FieldMaterial;

private:
	// 복제되는 필드 상태
	UPROPERTY(ReplicatedUsing = OnRep_FieldState)
	FVector FieldCenter = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_FieldState)
	FVector FieldExtent = FVector(50.0f);

	UPROPERTY(ReplicatedUsing = OnRep_FieldState)
	bool bFieldActive = false;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> FieldMID;

	FTimerHandle UpdateTimerHandle;
};
