// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMimicTargetZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Subsystem/CSMimicWorldSubsystem.h"

ACSMimicTargetZone::ACSMimicTargetZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	ZoneBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	ZoneBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(ZoneBox);

	// 구역 표시 메시 (시간정지 박스와 동일한 패턴)
	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMesh->SetupAttachment(ZoneBox);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticMeshRef(TEXT("/Script/Engine.StaticMesh'/Game/30_Mesh/StaticMesh/SM_Cube.SM_Cube'"));
	if (StaticMeshRef.Object)
	{
		ZoneMesh->SetStaticMesh(StaticMeshRef.Object);
	}

	// 빨간색 - 시간정지 머티리얼 재사용
	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialRef(TEXT("/Script/Engine.Material'/Game/31_Material/MAT_ChronoControl.MAT_ChronoControl'"));
	if (MaterialRef.Object)
	{
		ZoneMaterial = MaterialRef.Object;
	}
}

void ACSMimicTargetZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateZoneMesh();
}

void ACSMimicTargetZone::UpdateZoneMesh()
{
	if (ZoneMesh == nullptr || ZoneBox == nullptr) return;

	// SM_Cube 기본 크기 100x100x100 (피벗 코너) -> 박스 크기에 맞춰 스케일/센터 보정
	const float HalfSizeOfSide = 50.0f;
	const FVector MeshScale = ZoneBox->GetUnscaledBoxExtent() / HalfSizeOfSide;

	ZoneMesh->SetRelativeScale3D(MeshScale);
	ZoneMesh->SetRelativeLocation(FVector(-HalfSizeOfSide) * MeshScale);

	if (ZoneMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
		if (DynamicMaterial)
		{
			float OutBaseValue = 2.0f;
			DynamicMaterial->GetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue);
			DynamicMaterial->SetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue * MeshScale.X);
			ZoneMesh->SetMaterial(0, DynamicMaterial);
		}
	}
}

void ACSMimicTargetZone::BeginPlay()
{
	Super::BeginPlay();

	if (UCSMimicWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCSMimicWorldSubsystem>())
	{
		Subsystem->RegisterTargetZone(this);
	}
}

void ACSMimicTargetZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UCSMimicWorldSubsystem* Subsystem = World->GetSubsystem<UCSMimicWorldSubsystem>())
		{
			Subsystem->UnregisterTargetZone(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
