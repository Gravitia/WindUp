// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "CSProjectileGuideActor.generated.h"

UCLASS()
class CHRONOSPACE_API ACSProjectileGuideActor : public AActor
{
	GENERATED_BODY()
	
public:
	ACSProjectileGuideActor();

	// 가이드라인 업데이트
	UFUNCTION(BlueprintCallable, Category = "Projectile Guide")
	void UpdateGuideLine(const FVector& StartLocation, const FVector& EndLocation);

	// 가이드라인 표시/숨김
	UFUNCTION(BlueprintCallable, Category = "Projectile Guide")
	void SetGuideVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;

	// 스플라인 컴포넌트 (라인 그리기용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USplineComponent* GuideSpline;

	// 스플라인을 따라 생성될 메시 컴포넌트들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<UStaticMeshComponent*> GuideMeshComponents;

	// 가이드라인 끝점 메시 (충돌 지점 표시)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* EndPointMesh;

	// 가이드라인 메시 (기본 큐브나 실린더 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	UStaticMesh* GuideMesh;

	// 가이드라인 머티리얼
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	UMaterialInterface* GuideMaterial;

	// 가이드라인 두께
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	float GuideThickness = 2.0f;

	// 메시 세그먼트 길이 (긴 라인을 여러 메시로 분할)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	float SegmentLength = 100.0f;

private:
	void CreateGuideMeshes(float Distance);
	void ClearGuideMeshes();

};
