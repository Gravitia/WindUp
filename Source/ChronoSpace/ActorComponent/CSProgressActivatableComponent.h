// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSProgressActivatableComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSProgressActivatableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UCSProgressActivatableComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default|Progress")
    bool bStartActive = true;

    // 현재 상태(RepNotify)
    UPROPERTY(ReplicatedUsing = OnRep_ProgressActive, VisibleInstanceOnly, BlueprintReadOnly, Category = "Default|Progress")
    bool bIsProgressActive = false;

    UFUNCTION(BlueprintPure, Category = "Default|Progress")
    bool IsProgressActive() const { return bIsProgressActive; }

    // 서버에서만 호출하는 걸 추천 (권한 체크는 구현부에서)
    UFUNCTION(BlueprintCallable, Category = "Default|Progress")
    void SetProgressActive(bool bInActive);

protected:
    virtual void BeginPlay() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_ProgressActive();
		
};
