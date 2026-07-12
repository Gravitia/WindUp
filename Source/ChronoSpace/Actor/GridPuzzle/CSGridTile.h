// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSGridTile.generated.h"

UENUM(BlueprintType)
enum class ECSGridTileState : uint8
{
	Default,	// 아직 밟지 않은 칸
	Adjacent,	// 밟은 칸의 인접 4칸 - 미묘한 색 변화
	Stepped		// 밟은 칸 - 확실한 색 변화
};

/**
 * 격자 퍼즐 보드의 타일 한 칸.
 *
 * ACSGridBoard 가 BeginPlay 에서 격자로 깔아준다. (직접 레벨에 배치할 필요 없음)
 * 서버/클라 각자 로컬로 스폰되고, 상태 변화는 보드의 멀티캐스트로 동기화된다.
 * 플레이어(ACharacter)가 밟으면 서버에서 보드로 통지한다.
 */
UCLASS()
class CHRONOSPACE_API ACSGridTile : public AActor
{
	GENERATED_BODY()

public:
	ACSGridTile();

	// 타일 패드(발판) 두께(cm). 박스/레버가 패드 윗면 높이를 계산할 때 사용.
	static constexpr float PadThickness = 5.0f;

	// 보드가 스폰 직후(BeginPlay 전) 호출. 좌표/크기 세팅.
	void InitTile(class ACSGridBoard* InBoard, int32 InRow, int32 InCol, float InCellSize);

	// 보드 멀티캐스트가 모든 머신에서 호출. 상태 저장 + 색 적용.
	void ApplyTileState(ECSGridTileState NewState);

	ECSGridTileState GetTileState() const { return TileState; }
	int32 GetRow() const { return Row; }
	int32 GetCol() const { return Col; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tile")
	TObjectPtr<class USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tile")
	TObjectPtr<class UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Tile")
	TObjectPtr<class UBoxComponent> Trigger;

	// 상태별 색. 타일 BP 를 만들어 교체 가능.
	UPROPERTY(EditAnywhere, Category = "Tile")
	FLinearColor DefaultColor = FLinearColor(0.02f, 0.08f, 0.04f);

	UPROPERTY(EditAnywhere, Category = "Tile")
	FLinearColor AdjacentColor = FLinearColor(0.06f, 0.22f, 0.10f);

	UPROPERTY(EditAnywhere, Category = "Tile")
	FLinearColor SteppedColor = FLinearColor(0.10f, 0.85f, 0.35f);

protected:
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> TileMID;

	UPROPERTY()
	TObjectPtr<class ACSGridBoard> Board;

	ECSGridTileState TileState = ECSGridTileState::Default;

	int32 Row = -1;
	int32 Col = -1;
};
