// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/CSInteractionInterface.h"
#include "CSGridLever.generated.h"

/**
 * 박스 스폰 레버.
 *
 * 플레이어가 근처에서 E(상호작용) 키를 누르면 보드의 지정 칸(SpawnRow, SpawnCol)에
 * 박스(ACSGridBox)를 스폰한다. 스폰 칸이 박스/캐릭터로 막혀 있으면 스폰하지 않는다.
 * 상호작용은 기존 UCSPlayerInteractionComponent + ICSInteractionInterface 흐름을 그대로 사용한다.
 */
UCLASS()
class CHRONOSPACE_API ACSGridLever : public AActor, public ICSInteractionInterface
{
	GENERATED_BODY()

public:
	ACSGridLever();

	virtual void BeginInteraction() override;
	virtual void EndInteraction() override;
	virtual void Interact() override;

protected:
	virtual void BeginPlay() override;

	// 서버 전용. 지정 칸에 박스 스폰.
	void SpawnBox();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Used();

	void ApplyLeverColor();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<class USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<class UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<class USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	TObjectPtr<class UWidgetComponent> InteractionPromptComponent;

	// 스폰할 박스 클래스. 박스 BP 를 만들었으면 교체.
	UPROPERTY(EditAnywhere, Category = "Lever")
	TSubclassOf<class ACSGridBox> BoxClass;

	// 박스를 스폰할 보드와 격자 좌표. 보드가 없으면 레버 앞에 스폰한다.
	UPROPERTY(EditInstanceOnly, Category = "Lever")
	TObjectPtr<class ACSGridBoard> Board;

	UPROPERTY(EditAnywhere, Category = "Lever")
	int32 SpawnRow = 0;

	UPROPERTY(EditAnywhere, Category = "Lever")
	int32 SpawnCol = 0;

	// true 면 한 번만 사용 가능
	UPROPERTY(EditAnywhere, Category = "Lever")
	bool bSingleUse = true;

	// 스폰 위치 점유 검사에 쓰는 박스 절반 크기
	UPROPERTY(EditAnywhere, Category = "Lever")
	FVector SpawnCheckExtent = FVector(60.0f, 60.0f, 60.0f);

	UPROPERTY(EditAnywhere, Category = "Lever")
	FLinearColor IdleColor = FLinearColor(0.70f, 0.10f, 0.10f);

	UPROPERTY(EditAnywhere, Category = "Lever")
	FLinearColor UsedColor = FLinearColor(0.10f, 0.80f, 0.20f);

protected:
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> LeverMID;

	// Multicast 로만 전파하면 늦게 접속한 클라는 이미 당겨진 레버를 안 당겨진 색으로 본다
	UPROPERTY(ReplicatedUsing = OnRep_Used)
	bool bUsed = false;
};
