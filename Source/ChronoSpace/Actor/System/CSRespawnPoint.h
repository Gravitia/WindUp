// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CSRespawnPoint.generated.h"

UENUM(BlueprintType)
enum class ERespawnPlayerSlot : uint8
{
	Any,        // 아무 플레이어나
	Player1,    // 플레이어 1 전용
	Player2     // 플레이어 2 전용
};


UCLASS()
class CHRONOSPACE_API ACSRespawnPoint : public AActor
{
	GENERATED_BODY()
	
public:
    ACSRespawnPoint();

protected:
    virtual void BeginPlay() override;

public:
    // === Components ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* DirectionArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* RespawnMesh;

    // === Settings ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn Settings")
    FString ConnectedCheckPointID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn Settings")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn Settings")
    ERespawnPlayerSlot PlayerSlot = ERespawnPlayerSlot::Any;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn Settings")
    bool bIsActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "Respawn State")
    FString RespawnPointID;

public:
    // === Core Functions ===
    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    bool SpawnPlayerHere(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    bool CanSpawnHere() const;

    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    void SetActive(bool bNewActive);

    UFUNCTION(BlueprintCallable, Category = "Respawn Point")
    FString GetRespawnPointID() const { return RespawnPointID; }

    // === Blueprint Events ===
    UFUNCTION(BlueprintImplementableEvent, Category = "Respawn Events")
    void OnPlayerSpawned(APawn* Player);

    UFUNCTION(BlueprintImplementableEvent, Category = "Respawn Events")
    void OnRespawnPointActivated();

    UFUNCTION(BlueprintImplementableEvent, Category = "Respawn Events")
    void OnRespawnPointDeactivated();

private:
    void GenerateID();
    int32 GetPlayerSlotNumber(APawn* Player) const;
    class ACSGameMode* GetCSGameMode() const;

};
