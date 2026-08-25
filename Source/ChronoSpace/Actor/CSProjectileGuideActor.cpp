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
	if (!GuideMesh || !GuideSpline || Distance <= 0)
	{
		SetGuideVisible(false);
		return;
	}

	const int32 NumSegments = FMath::CeilToInt(Distance / SegmentLength);
	const float SplineLength = GuideSpline->GetSplineLength();

	// 부족한 만큼만 새로 만들고 나머지는 재사용한다.
	// 예전엔 호출마다 전부 DestroyComponent -> NewObject -> RegisterComponent 를 반복했고,
	// 이 함수는 조준하는 동안 매 프레임 불린다 (렌더 프록시 재생성 + GC 압박 + 프레임 스파이크).
	while (GuideMeshComponents.Num() < NumSegments)
	{
		UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(this);
		NewMeshComp->SetupAttachment(RootComponent);
		NewMeshComp->SetStaticMesh(GuideMesh);
		NewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (GuideMaterial)
		{
			NewMeshComp->SetMaterial(0, GuideMaterial);
		}

		NewMeshComp->RegisterComponent();
		GuideMeshComponents.Add(NewMeshComp);
	}

	for (int32 i = 0; i < GuideMeshComponents.Num(); ++i)
	{
		UStaticMeshComponent* MeshComp = GuideMeshComponents[i];
		if (!IsValid(MeshComp)) continue;

		// 남는 세그먼트는 숨겨만 둔다 (다음 프레임에 다시 쓸 수 있게)
		if (i >= NumSegments)
		{
			MeshComp->SetVisibility(false);
			continue;
		}

		// 세그먼트 중심에 배치한다.
		// 예전엔 i/(N-1) 로 양 끝 포함 균등 배치하면서 길이는 SegmentLength 로 스케일해
		// 간격(Distance/(N-1))과 길이가 맞지 않아 겹치거나 벌어졌다.
		const float SegmentActualLength = FMath::Max(FMath::Min(SegmentLength, Distance - (i * SegmentLength)), 1.0f);
		const float CenterDistance = FMath::Clamp(i * SegmentLength + SegmentActualLength * 0.5f, 0.0f, SplineLength);

		const FVector Location = GuideSpline->GetLocationAtDistanceAlongSpline(CenterDistance, ESplineCoordinateSpace::World);
		const FVector Direction = GuideSpline->GetDirectionAtDistanceAlongSpline(CenterDistance, ESplineCoordinateSpace::World);

		MeshComp->SetVisibility(true);
		MeshComp->SetWorldLocation(Location);
		MeshComp->SetWorldRotation(Direction.Rotation());
		MeshComp->SetWorldScale3D(FVector(
			SegmentActualLength / 100.0f, // X축 (길이)
			GuideThickness / 100.0f,      // Y축 (두께)
			GuideThickness / 100.0f       // Z축 (두께)
		));
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

