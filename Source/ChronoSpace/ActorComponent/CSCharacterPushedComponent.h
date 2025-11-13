// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSCharacterPushedComponent.generated.h"

// Character who has this component can be pushed by UCSPushingCharacterComponent
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSCharacterPushedComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCSCharacterPushedComponent();

		
};
