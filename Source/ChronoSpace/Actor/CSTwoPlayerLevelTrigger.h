// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSTwoPlayerLevelTrigger.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACharacter;

UCLASS()
class CHRONOSPACE_API ACSTwoPlayerLevelTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACSTwoPlayerLevelTrigger();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_PlayerCount();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartTransition();

	void ApplyLightVisibility();
	void ExecuteServerTravel();
	void PreloadTargetLevel();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Light1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Light2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FString TargetLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "0.0"))
	float FadeDuration;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerCount)
	int32 PlayerCount;

	UPROPERTY()
	TArray<TObjectPtr<ACharacter>> PlayersInTrigger;

	bool bTransitionStarted;
	bool bPreloadStarted;
};
