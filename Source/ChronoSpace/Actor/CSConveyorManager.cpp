// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorManager.h"
#include "Actor/CSConveyorPlatform.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ACSConveyorManager::ACSConveyorManager()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	ConveyorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConveyorISM"));
	ConveyorISM->SetupAttachment(RootComponent);
	ConveyorISM->SetMobility(EComponentMobility::Movable);
	// 기존: NoCollision
	ConveyorISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 가장 쉬운 방법: 프로필 사용
	ConveyorISM->SetCollisionProfileName(TEXT("BlockAll"));

}

void ACSConveyorManager::BeginPlay()
{
	Super::BeginPlay();

	InitializePlatformsFromAssigned();
	// BuildVisual(); // 필요하면 호출

	TargetProgress = RepProgress;
	SmoothedProgress = RepProgress;
}

void ACSConveyorManager::InitializePlatformsFromAssigned()
{
	Platforms.Empty();

	// 1) null 제거하면서 순서 유지
	for (ACSConveyorPlatform* P : AssignedPlatforms)
	{
		if (!P)
		{
			continue;
		}

		Platforms.Add(P);
	}

	// 2) 인덱스 기반 오프셋 적용
	for (int32 i = 0; i < Platforms.Num(); ++i)
	{
		ACSConveyorPlatform* P = Platforms[i];
		if (!P)
		{
			continue;
		}

		P->SetManager(this);
		P->SetIndexOffset((float)i * ConveyorSpacing);
	}

	// 3) 총 길이는 "플랫폼 개수 * spacing"
	TotalLength = (float)Platforms.Num() * ConveyorSpacing;

	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		TotalLength = 0.f;
	}
}


void ACSConveyorManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// ===== Server =====
	if (HasAuthority())
	{
		RepProgress -= MoveSpeed * DeltaSeconds;
		RepProgress = FMath::Fmod(RepProgress, TotalLength);
		if (RepProgress < 0.f)
		{
			RepProgress += TotalLength;
		}

		TargetProgress = RepProgress;
	}

	// ===== Client smoothing =====
	const float Delta = FMath::Abs(TargetProgress - SmoothedProgress);

	if (Delta > TotalLength * 0.5f)
	{
		SmoothedProgress = TargetProgress;
	}
	else
	{
		SmoothedProgress = FMath::FInterpTo(
			SmoothedProgress,
			TargetProgress,
			DeltaSeconds,
			InterpSpeed
		);
	}
}

void ACSConveyorManager::OnRep_RepProgress()
{
	TargetProgress = RepProgress;
}

void ACSConveyorManager::BuildVisual()
{
	if (!ConveyorISM || !ConveyorMesh)
	{
		return;
	}

	// AssignedPlatforms 기준으로 유효 개수 계산(순서 유지)
	int32 ValidCount = 0;
	for (ACSConveyorPlatform* P : AssignedPlatforms)
	{
		if (P)
		{
			++ValidCount;
		}
	}

	ConveyorISM->ClearInstances();

	if (ValidCount <= 0)
	{
		ConveyorISM->MarkRenderStateDirty();
		return;
	}

	ConveyorISM->SetStaticMesh(ConveyorMesh);

	if (ConveyorMaterial)
	{
		ConveyorISM->SetMaterial(0, ConveyorMaterial);
	}

	for (int32 i = 0; i < ValidCount; ++i)
	{
		FTransform T;
		T.SetLocation(FVector(0.f, (float)i * ConveyorSpacing, 0.f));
		T.SetRotation(FQuat::Identity);
		T.SetScale3D(FVector(1.f));

		ConveyorISM->AddInstance(T);
	}

	ConveyorISM->MarkRenderStateDirty();
}

void ACSConveyorManager::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSConveyorManager, RepProgress);
}
