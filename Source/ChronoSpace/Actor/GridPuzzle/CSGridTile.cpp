// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/GridPuzzle/CSGridTile.h"
#include "Actor/GridPuzzle/CSGridBoard.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"

ACSGridTile::ACSGridTile()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 발판 메시 (엔진 기본 큐브를 얇게 눌러서 사용)
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(Root);
	PadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, PadThickness * 0.5f));
	PadMesh->SetRelativeScale3D(FVector(1.9f, 1.9f, PadThickness / 100.0f));
	PadMesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshRef(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshRef.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeMeshRef.Object);
	}

	// Cube 애셋의 기본 머티리얼(WorldGridMaterial)에는 Color 파라미터가 없어서
	// 색 변경이 되도록 BasicShapeMaterial 로 교체한다.
	static ConstructorHelpers::FObjectFinder<UMaterial> BaseMaterialRef(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterialRef.Succeeded())
	{
		PadMesh->SetMaterial(0, BaseMaterialRef.Object);
	}

	// 밟음 감지 트리거
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetBoxExtent(FVector(90.0f, 90.0f, 50.0f));
	Trigger->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	Trigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
}

void ACSGridTile::InitTile(ACSGridBoard* InBoard, int32 InRow, int32 InCol, float InCellSize)
{
	Board = InBoard;
	Row = InRow;
	Col = InCol;

	// 칸 크기에 맞게 발판/트리거 크기 조정 (칸 사이 살짝 틈을 둔다)
	const float PadScaleXY = InCellSize * 0.95f / 100.0f;
	PadMesh->SetRelativeScale3D(FVector(PadScaleXY, PadScaleXY, PadThickness / 100.0f));
	Trigger->SetBoxExtent(FVector(InCellSize * 0.45f, InCellSize * 0.45f, 50.0f));
}

void ACSGridTile::BeginPlay()
{
	Super::BeginPlay();

	TileMID = PadMesh->CreateAndSetMaterialInstanceDynamic(0);
	ApplyTileState(TileState);

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACSGridTile::OnTriggerBeginOverlap);
}

void ACSGridTile::ApplyTileState(ECSGridTileState NewState)
{
	TileState = NewState;

	if (TileMID == nullptr && PadMesh)
	{
		TileMID = PadMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (TileMID == nullptr)
	{
		return;
	}

	FLinearColor Color = DefaultColor;
	switch (TileState)
	{
	case ECSGridTileState::Adjacent:
		Color = AdjacentColor;
		break;
	case ECSGridTileState::Stepped:
		Color = SteppedColor;
		break;
	default:
		break;
	}

	TileMID->SetVectorParameterValue(TEXT("Color"), Color);
}

void ACSGridTile::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	// 타일은 각 머신에서 로컬로 스폰되므로 HasAuthority 대신 보드(레벨 배치 액터) 권한으로 판단한다.
	if (Board == nullptr || !Board->HasAuthority())
	{
		return;
	}

	if (Cast<ACharacter>(OtherActor) == nullptr)
	{
		return;
	}

	Board->NotifyTileStepped(this);
}
