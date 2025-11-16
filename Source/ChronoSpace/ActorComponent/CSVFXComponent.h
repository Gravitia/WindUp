// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSVFXComponent.generated.h"

UENUM(BlueprintType)
enum class EWorldVFX : uint8
{
	NONE,
	EFFECT_DEAD_0,
};

UENUM(BlueprintType)
enum class EActorAttachedVFX : uint8
{
	NONE,
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CHRONOSPACE_API UCSVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCSVFXComponent();

public:
	UFUNCTION( BlueprintCallable )
	void PlayWorldVFX( EWorldVFX VFX );
	void PlayActorAttackedVFX(EActorAttachedVFX VFX, FName Socket = NAME_None);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TMap< EWorldVFX, TSoftObjectPtr< class UNiagaraSystem > > WorldVFXMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TMap< EActorAttachedVFX, TSoftObjectPtr< class UNiagaraSystem > > ActorAttackedVFXMap;
};
