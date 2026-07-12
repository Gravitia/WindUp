// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSGravityAnchorItem.generated.h"

/**
 * 중력 반전 앵커 아이템.
 * 같은 GroupChannel의 아이템이 4개(CSReverseGravityField의 RequiredItemCount) 배치되면,
 * 아이템들의 XY 위치를 감싸는 영역에 중력 반전 필드가 활성화된다.
 * 물리 시뮬레이션이 켜져 있어 플레이어가 밀어서 옮길 수 있다.
 */
UCLASS()
class CHRONOSPACE_API ACSGravityAnchorItem : public AActor
{
	GENERATED_BODY()

public:
	ACSGravityAnchorItem();

	FORCEINLINE FName GetGroupChannel() const { return GroupChannel; }

protected:
	// 중력 반전 필드와 연결되는 채널 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity Anchor")
	FName GroupChannel = TEXT("Default");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gravity Anchor")
	TObjectPtr<class UStaticMeshComponent> Mesh;
};
