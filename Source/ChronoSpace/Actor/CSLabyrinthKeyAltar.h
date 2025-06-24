// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/CSInteractionInterface.h"
#include "CSLabyrinthKeyAltar.generated.h"

UCLASS()
class CHRONOSPACE_API ACSLabyrinthKeyAltar : public AActor, public ICSInteractionInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACSLabyrinthKeyAltar();

	virtual void BeginInteraction() override;
	virtual void EndInteraction() override;
	virtual void Interact() override;

protected:
	virtual void BeginPlay() override;

protected:
	void ChangeLevel();

	UPROPERTY(VisibleAnywhere, Category = "Trigger", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> SphereTrigger;

	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	TObjectPtr<class UStaticMeshComponent> StaticMeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<class UStaticMesh> StaticMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UWidgetComponent> InteractionPromptComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TSoftClassPtr<UUserWidget> InteractionPromptWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	float TriggerRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	int32 RequiredKeyCount;
};
