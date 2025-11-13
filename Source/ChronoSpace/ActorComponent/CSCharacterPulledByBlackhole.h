// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSCharacterPulledByBlackhole.generated.h"

// Character who has this component can be pulled by ACSBlackHole
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSCharacterPulledByBlackhole : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCSCharacterPulledByBlackhole();

};
