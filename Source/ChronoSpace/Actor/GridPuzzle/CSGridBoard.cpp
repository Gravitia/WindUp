// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/GridPuzzle/CSGridBoard.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

ACSGridBoard::ACSGridBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// 보드가 플레이어와 멀어도 타일 상태 멀티캐스트가 누락되지 않도록
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	GridArea = CreateDefaultSubobject<UBoxComponent>(TEXT("GridArea"));
	GridArea->SetupAttachment(Root);
	GridArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridArea->SetHiddenInGame(true);

	TileClass = ACSGridTile::StaticClass();
}

void ACSGridBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 격자 영역이 보이도록 가이드 박스를 격자 크기에 맞춘다.
	const FVector Extent(Rows * CellSize * 0.5f, Cols * CellSize * 0.5f, 5.0f);
	GridArea->SetBoxExtent(Extent);
	GridArea->SetRelativeLocation(Extent);
}

void ACSGridBoard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSGridBoard, bCompleted);
}

void ACSGridBoard::BeginPlay()
{
	Super::BeginPlay();

	SpawnTiles();
}

void ACSGridBoard::SpawnTiles()
{
	if (TileClass == nullptr)
	{
		TileClass = ACSGridTile::StaticClass();
	}

	Tiles.Reset();
	Tiles.Reserve(Rows * Cols);

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const FTransform TileTransform(GetActorRotation(), CellToWorld(Row, Col));

			ACSGridTile* Tile = GetWorld()->SpawnActorDeferred<ACSGridTile>(TileClass, TileTransform, this);
			if (Tile)
			{
				Tile->InitTile(this, Row, Col, CellSize);
				Tile->FinishSpawning(TileTransform);
				Tile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			}

			Tiles.Add(Tile);
		}
	}
}

bool ACSGridBoard::IsValidCell(int32 Row, int32 Col) const
{
	return Row >= 0 && Row < Rows && Col >= 0 && Col < Cols;
}

FVector ACSGridBoard::CellToWorld(int32 Row, int32 Col) const
{
	const FVector Local((Row + 0.5f) * CellSize, (Col + 0.5f) * CellSize, 0.0f);
	return GetActorTransform().TransformPosition(Local);
}

bool ACSGridBoard::WorldToCell(const FVector& WorldLocation, int32& OutRow, int32& OutCol) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	OutRow = FMath::FloorToInt(Local.X / CellSize);
	OutCol = FMath::FloorToInt(Local.Y / CellSize);
	return IsValidCell(OutRow, OutCol);
}

FVector ACSGridBoard::SnapToCellCenter(const FVector& WorldLocation) const
{
	int32 Row = 0;
	int32 Col = 0;
	WorldToCell(WorldLocation, Row, Col);
	Row = FMath::Clamp(Row, 0, Rows - 1);
	Col = FMath::Clamp(Col, 0, Cols - 1);

	FVector Center = CellToWorld(Row, Col);
	Center.Z = WorldLocation.Z;
	return Center;
}

float ACSGridBoard::GetTileTopZ(int32 Row, int32 Col) const
{
	return CellToWorld(Row, Col).Z + ACSGridTile::PadThickness;
}

void ACSGridBoard::NotifyTileStepped(ACSGridTile* Tile)
{
	if (!HasAuthority() || Tile == nullptr)
	{
		return;
	}

	const int32 Row = Tile->GetRow();
	const int32 Col = Tile->GetCol();

	// 이전에 표시했던 인접 칸 원복 (누적 모드가 아닐 때)
	if (!bPersistentAdjacent)
	{
		for (ACSGridTile* Prev : RecentAdjacentTiles)
		{
			if (Prev && Prev->GetTileState() == ECSGridTileState::Adjacent)
			{
				NetMulticastSetTileState(Prev->GetRow(), Prev->GetCol(), ECSGridTileState::Default);
			}
		}
	}
	RecentAdjacentTiles.Reset();

	// 밟은 칸은 확실한 색 변화 (한 번 밟으면 유지)
	if (Tile->GetTileState() != ECSGridTileState::Stepped)
	{
		NetMulticastSetTileState(Row, Col, ECSGridTileState::Stepped);
		++SteppedCount;
	}

	// 인접 4칸은 미묘한 색 변화
	const int32 DeltaRow[] = { 1, -1, 0, 0 };
	const int32 DeltaCol[] = { 0, 0, 1, -1 };
	for (int32 i = 0; i < 4; ++i)
	{
		ACSGridTile* Neighbor = GetTile(Row + DeltaRow[i], Col + DeltaCol[i]);
		if (Neighbor && Neighbor->GetTileState() == ECSGridTileState::Default)
		{
			NetMulticastSetTileState(Neighbor->GetRow(), Neighbor->GetCol(), ECSGridTileState::Adjacent);
			RecentAdjacentTiles.Add(Neighbor);
		}
	}

	// 모든 칸을 밟으면 완료
	if (!bCompleted && SteppedCount >= Rows * Cols)
	{
		bCompleted = true;
		UE_LOG(LogCS, Log, TEXT("[GridBoard] %s completed"), *GetName());
		NetMulticastBoardCompleted();
	}
}

void ACSGridBoard::ResetBoard()
{
	if (!HasAuthority())
	{
		return;
	}

	bCompleted = false;
	SteppedCount = 0;
	RecentAdjacentTiles.Reset();

	NetMulticastResetBoard();
}

void ACSGridBoard::NetMulticastSetTileState_Implementation(int32 Row, int32 Col, ECSGridTileState NewState)
{
	if (ACSGridTile* Tile = GetTile(Row, Col))
	{
		Tile->ApplyTileState(NewState);
	}
}

void ACSGridBoard::NetMulticastBoardCompleted_Implementation()
{
	OnBoardCompleted();
}

void ACSGridBoard::NetMulticastResetBoard_Implementation()
{
	for (ACSGridTile* Tile : Tiles)
	{
		if (Tile)
		{
			Tile->ApplyTileState(ECSGridTileState::Default);
		}
	}
}

ACSGridTile* ACSGridBoard::GetTile(int32 Row, int32 Col) const
{
	if (!IsValidCell(Row, Col))
	{
		return nullptr;
	}

	const int32 Index = Row * Cols + Col;
	return Tiles.IsValidIndex(Index) ? Tiles[Index].Get() : nullptr;
}
