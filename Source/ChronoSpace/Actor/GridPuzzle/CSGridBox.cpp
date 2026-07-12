// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/GridPuzzle/CSGridBox.h"
#include "Actor/GridPuzzle/CSGridBoard.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "ChronoSpace.h"

ACSGridBox::ACSGridBox()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetNotifyRigidBodyCollision(true);
	// 200 짜리 칸 기준 170cm 박스
	Mesh->SetRelativeScale3D(FVector(1.7f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshRef(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshRef.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMeshRef.Object);
	}

	// Cube 애셋의 기본 머티리얼(WorldGridMaterial)에는 Color 파라미터가 없어서
	// 색 변경이 되도록 BasicShapeMaterial 로 교체한다.
	static ConstructorHelpers::FObjectFinder<UMaterial> BaseMaterialRef(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterialRef.Succeeded())
	{
		Mesh->SetMaterial(0, BaseMaterialRef.Object);
	}
}

void ACSGridBox::BeginPlay()
{
	Super::BeginPlay();

	// 밀리는 박스 / 고정 박스를 색으로 구분
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), bPushable ? PushableColor : ImmovableColor);
	}

	Mesh->OnComponentHit.AddDynamic(this, &ACSGridBox::OnMeshHit);

	if (HasAuthority())
	{
		if (Board == nullptr)
		{
			Board = FindNearestBoard();
		}

		// 보드 위에 놓여 있으면 칸 중심에 스냅 (레벨 배치를 대충 해도 됨)
		int32 Row = 0;
		int32 Col = 0;
		if (Board && Board->WorldToCell(GetActorLocation(), Row, Col))
		{
			SetActorLocation(Board->SnapToCellCenter(GetActorLocation()));
		}
	}
}

void ACSGridBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoving)
	{
		SetActorTickEnabled(false);
		return;
	}

	MoveElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.0f, 1.0f);
	SetActorLocation(FMath::Lerp(MoveStart, MoveTarget, FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f)));

	if (Alpha >= 1.0f)
	{
		bMoving = false;
		SetActorTickEnabled(false);
	}
}

void ACSGridBox::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bPushable || bMoving)
	{
		return;
	}

	ACharacter* Pusher = Cast<ACharacter>(OtherActor);
	if (Pusher == nullptr)
	{
		return;
	}

	TryPush(Pusher);
}

void ACSGridBox::TryPush(ACharacter* Pusher)
{
	FVector Delta = GetActorLocation() - Pusher->GetActorLocation();
	Delta.Z = 0.0f;
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// 미는 방향을 격자 주축(상하좌우) 중 하나로 스냅해서 목표 칸을 정한다.
	FVector TargetCenter;
	if (Board)
	{
		int32 Row = 0;
		int32 Col = 0;
		if (!Board->WorldToCell(GetActorLocation(), Row, Col))
		{
			return;
		}

		const FVector LocalDir = Board->GetActorTransform().InverseTransformVectorNoScale(Delta);
		int32 DeltaRow = 0;
		int32 DeltaCol = 0;
		if (FMath::Abs(LocalDir.X) >= FMath::Abs(LocalDir.Y))
		{
			DeltaRow = LocalDir.X > 0.0f ? 1 : -1;
		}
		else
		{
			DeltaCol = LocalDir.Y > 0.0f ? 1 : -1;
		}

		if (!Board->IsValidCell(Row + DeltaRow, Col + DeltaCol))
		{
			return;
		}

		TargetCenter = Board->CellToWorld(Row + DeltaRow, Col + DeltaCol);
	}
	else
	{
		FVector Dir;
		if (FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y))
		{
			Dir = FVector(Delta.X > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
		}
		else
		{
			Dir = FVector(0.0f, Delta.Y > 0.0f ? 1.0f : -1.0f, 0.0f);
		}

		TargetCenter = GetActorLocation() + Dir * FallbackCellSize;
	}
	TargetCenter.Z = GetActorLocation().Z;

	// 캐릭터가 실제로 미는 방향으로 이동 중일 때만 밀린다. (스치기만 해도 밀리는 것 방지)
	FVector PushDir = TargetCenter - GetActorLocation();
	PushDir.Z = 0.0f;
	PushDir.Normalize();

	FVector Velocity = Pusher->GetVelocity();
	Velocity.Z = 0.0f;
	if (Velocity.Size() < MinPushSpeed || FVector::DotProduct(Velocity.GetSafeNormal(), PushDir) < 0.5f)
	{
		return;
	}

	if (!IsLocationFree(TargetCenter))
	{
		return;
	}

	MoveStart = GetActorLocation();
	MoveTarget = TargetCenter;
	MoveElapsed = 0.0f;
	bMoving = true;
	SetActorTickEnabled(true);
}

bool ACSGridBox::IsLocationFree(const FVector& Center) const
{
	// 박스보다 살짝 작은 영역으로 검사해서 바닥/타일에는 걸리지 않게 한다.
	const FVector Extent = Mesh->Bounds.BoxExtent * 0.8f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CSGridBoxPush), false, this);
	return !GetWorld()->OverlapBlockingTestByChannel(Center, GetActorQuat(), ECC_WorldDynamic, FCollisionShape::MakeBox(Extent), Params);
}

ACSGridBoard* ACSGridBox::FindNearestBoard() const
{
	ACSGridBoard* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ACSGridBoard> It(GetWorld()); It; ++It)
	{
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}

	return Nearest;
}
