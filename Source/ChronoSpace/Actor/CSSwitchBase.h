// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/CSInteractionInterface.h"
#include "CSSwitchBase.generated.h"

UCLASS()
class CHRONOSPACE_API ACSSwitchBase : public AActor, public ICSInteractionInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACSSwitchBase();

	virtual void BeginInteraction() override;
	virtual void EndInteraction() override;
	virtual void Interact() override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsInteracted() const { return bIsInteracted; }

protected:
	/** 서버에서 bIsInteracted 를 바꾼 직후 호출 - 리슨 호스트는 OnRep 을 받지 않으므로 여기서 직접 적용한다 */
	void SetMaterial();

	UFUNCTION()
	void OnRep_IsInteracted();

	/** 머티리얼 적용 (모든 머신에서 로컬 실행) */
	void ApplyMaterial();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<class UCSSwitchBaseData> Data;

protected:
	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> StaticMeshComp;

	UPROPERTY()
	TObjectPtr<class USphereComponent> Trigger;

	UPROPERTY()
	TObjectPtr<class UWidgetComponent> InteractionPromptComponent;

	// Multicast RPC 로만 전파하면 늦게 접속한 클라는 항상 기본 상태(OFF)를 본다.
	// 복제 프로퍼티 + 서버에서 OnRep 수동 호출이 올바른 방법.
	UPROPERTY(ReplicatedUsing = OnRep_IsInteracted)
	bool bIsInteracted;
};
