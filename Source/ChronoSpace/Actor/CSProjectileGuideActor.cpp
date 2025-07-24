// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSProjectileGuideActor.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACSProjectileGuideActor::ACSProjectileGuideActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // 로컬 전용 (시각적 효과)

	// 루트 컴포넌트
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 스플라인 컴포넌트
	GuideSpline = CreateDefaultSubobject<USplineComponent>(TEXT("GuideSpline"));
	GuideSpline->SetupAttachment(RootComponent);


	// 끝점 메시
	EndPointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EndPointMesh"));
	EndPointMesh->SetupAttachment(RootComponent);
	EndPointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 기본 설정
	GuideThickness = 2.0f;
	SegmentLength = 100.0f;

	// 기본 메시 설정 (개발용 - 나중에 Blueprint에서 교체)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		GuideMesh = CubeMesh.Object;
		EndPointMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ACSProjectileGuideActor::BeginPlay()
{
	Super::BeginPlay();

	

	if (EndPointMesh)
	{
		EndPointMesh->SetWorldScale3D(FVector(0.2f)); // 조금 더 큰 포인트
	}
}

void ACSProjectileGuideActor::UpdateGuideLine(const FVector& StartLocation, const FVector& EndLocation)
{
	if (!GuideSpline)
	{
		return;
	}

	// 스플라인 포인트 설정
	GuideSpline->ClearSplinePoints();
	GuideSpline->AddSplinePoint(StartLocation, ESplineCoordinateSpace::World);
	GuideSpline->AddSplinePoint(EndLocation, ESplineCoordinateSpace::World);
	GuideSpline->UpdateSpline();

	

	if (EndPointMesh)
	{
		EndPointMesh->SetWorldLocation(EndLocation);
	}

	// 가이드라인 메시 생성/업데이트
	float Distance = FVector::Dist(StartLocation, EndLocation);
	CreateGuideMeshes(Distance);
}

void ACSProjectileGuideActor::CreateGuideMeshes(float Distance)
{
	// 기존 메시 컴포넌트 정리
	ClearGuideMeshes();

	if (!GuideMesh || !GuideSpline || Distance <= 0)
	{
		return;
	}

	// 필요한 세그먼트 수 계산
	int32 NumSegments = FMath::CeilToInt(Distance / SegmentLength);

	for (int32 i = 0; i < NumSegments; i++)
	{
		// 메시 컴포넌트 생성
		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
		MeshComp->SetupAttachment(RootComponent);
		MeshComp->SetStaticMesh(GuideMesh);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (GuideMaterial)
		{
			MeshComp->SetMaterial(0, GuideMaterial);
		}

		// 스플라인을 따라 위치/회전 설정
		float Alpha = (float)i / (float)FMath::Max(1, NumSegments - 1);
		FVector Location = GuideSpline->GetLocationAtDistanceAlongSpline(
			Alpha * GuideSpline->GetSplineLength(),
			ESplineCoordinateSpace::World
		);

		FVector Direction = GuideSpline->GetDirectionAtDistanceAlongSpline(
			Alpha * GuideSpline->GetSplineLength(),
			ESplineCoordinateSpace::World
		);

		// 메시 위치/회전 설정
		MeshComp->SetWorldLocation(Location);
		MeshComp->SetWorldRotation(Direction.Rotation());

		// 메시 스케일 설정 (길이는 세그먼트 길이, 두께는 설정값)
		float SegmentActualLength = FMath::Min(SegmentLength, Distance - (i * SegmentLength));
		MeshComp->SetWorldScale3D(FVector(
			SegmentActualLength / 100.0f, // X축 (길이)
			GuideThickness / 100.0f,      // Y축 (두께)
			GuideThickness / 100.0f       // Z축 (두께)
		));

		// 컴포넌트 등록
		MeshComp->RegisterComponent();
		GuideMeshComponents.Add(MeshComp);
	}
}

void ACSProjectileGuideActor::ClearGuideMeshes()
{
	for (UStaticMeshComponent* MeshComp : GuideMeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->DestroyComponent();
		}
	}
	GuideMeshComponents.Empty();
}

void ACSProjectileGuideActor::SetGuideVisible(bool bVisible)
{
	// 모든 메시 컴포넌트의 가시성 설정

	if (EndPointMesh)
	{
		EndPointMesh->SetVisibility(bVisible);
	}

	for (UStaticMeshComponent* MeshComp : GuideMeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetVisibility(bVisible);
		}
	}
}

