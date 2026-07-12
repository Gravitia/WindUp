// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Actor/GridPuzzle/CSGridTile.h"
#include "CSGridBoard.generated.h"

class UBoxComponent;

/**
 * 네모 격자 퍼즐 보드. (터치 타일 퍼즐 / PUSH PUSH 테스트용)
 *
 * 레벨에 보드 하나만 놓고 Rows / Cols / CellSize 를 세팅하면
 * BeginPlay 에서 타일(ACSGridTile)을 격자로 깔아준다.
 *  - 플레이어가 밟은 타일은 색이 바뀌고, 인접 4칸은 미묘하게 색이 바뀐다.
 *  - 모든 타일을 밟으면 OnBoardCompleted 이벤트 (BP 에서 오브젝트 활성화 연결)
 *  - 박스(ACSGridBox) / 레버(ACSGridLever)가 격자 좌표 변환에 이 보드를 사용한다.
 *
 * 격자 좌표: 보드 로컬 X 축 = Row, Y 축 = Col. 보드 위치가 (0,0) 칸의 모서리.
 */
UCLASS()
class CHRONOSPACE_API ACSGridBoard : public AActor
{
	GENERATED_BODY()

public:
	ACSGridBoard();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ==== 격자 <-> 월드 변환 ====
	bool IsValidCell(int32 Row, int32 Col) const;

	// 칸 중심 월드 좌표 (보드 바닥 높이)
	FVector CellToWorld(int32 Row, int32 Col) const;

	// 월드 좌표 -> 격자 좌표. 보드 밖이면 false.
	bool WorldToCell(const FVector& WorldLocation, int32& OutRow, int32& OutCol) const;

	// 가장 가까운 칸 중심으로 XY 스냅 (Z 는 유지)
	FVector SnapToCellCenter(const FVector& WorldLocation) const;

	// 타일 패드 윗면의 월드 Z (박스/레버 스폰 높이 기준)
	float GetTileTopZ(int32 Row, int32 Col) const;

	float GetCellSize() const { return CellSize; }

	// 타일이 밟혔을 때 타일 -> 보드 통지. (서버 전용)
	void NotifyTileStepped(ACSGridTile* Tile);

	// 보드 전체를 초기 상태로 되돌린다. (서버 권한)
	UFUNCTION(BlueprintCallable, Category = "GridBoard")
	void ResetBoard();

	UFUNCTION(BlueprintPure, Category = "GridBoard")
	bool IsBoardCompleted() const { return bCompleted; }

protected:
	virtual void BeginPlay() override;

	// 모든 타일을 밟았을 때. 성공 시 오브젝트 활성화 등을 BP 에서 여기에 연결한다. (모든 머신에서 호출)
	UFUNCTION(BlueprintImplementableEvent, Category = "GridBoard", meta = (DisplayName = "On Board Completed"))
	void OnBoardCompleted();

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastSetTileState(int32 Row, int32 Col, ECSGridTileState NewState);

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastBoardCompleted();

	UFUNCTION(NetMulticast, Reliable)
	void NetMulticastResetBoard();

	void SpawnTiles();
	ACSGridTile* GetTile(int32 Row, int32 Col) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "GridBoard")
	TObjectPtr<USceneComponent> Root;

	// 에디터에서 격자 영역을 보여주는 가이드 박스 (게임 중에는 숨김)
	UPROPERTY(VisibleAnywhere, Category = "GridBoard")
	TObjectPtr<UBoxComponent> GridArea;

	UPROPERTY(EditAnywhere, Category = "GridBoard", meta = (ClampMin = "1"))
	int32 Rows = 5;

	UPROPERTY(EditAnywhere, Category = "GridBoard", meta = (ClampMin = "1"))
	int32 Cols = 5;

	UPROPERTY(EditAnywhere, Category = "GridBoard", meta = (ClampMin = "50.0"))
	float CellSize = 200.0f;

	// 타일 BP 를 따로 만들었으면 교체. 비워두면 C++ 기본 타일 사용.
	UPROPERTY(EditAnywhere, Category = "GridBoard")
	TSubclassOf<ACSGridTile> TileClass;

	// true: 인접(미묘한 색) 표시가 누적된다 / false: 마지막으로 밟은 칸 주변만 표시된다.
	UPROPERTY(EditAnywhere, Category = "GridBoard")
	bool bPersistentAdjacent = false;

protected:
	// 타일은 서버/클라 각자 로컬로 깔고, 상태 변화만 멀티캐스트로 맞춘다. 인덱스 = Row * Cols + Col.
	UPROPERTY()
	TArray<TObjectPtr<ACSGridTile>> Tiles;

	// 마지막으로 밟은 칸 주변에 인접 표시 중인 타일들. 서버 전용.
	UPROPERTY()
	TArray<TObjectPtr<ACSGridTile>> RecentAdjacentTiles;

	UPROPERTY(Replicated)
	bool bCompleted = false;

	// 밟은 타일 수. 서버 전용.
	int32 SteppedCount = 0;
};
