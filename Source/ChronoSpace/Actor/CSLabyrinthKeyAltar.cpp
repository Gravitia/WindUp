// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSLabyrinthKeyAltar.h"
#include "Character/CSCharacterPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Physics/CSCollision.h"
#include "Subsystem/CSLabyrinthKeyWorldSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "ChronoSpace.h"
#include "Engine/World.h"

// Sets default values
ACSLabyrinthKeyAltar::ACSLabyrinthKeyAltar()
{
	bReplicates = true;

	// Static Mesh
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	RootComponent = StaticMeshComp;
	StaticMeshComp->SetCollisionProfileName(CPROFILE_CSCAPSULE);
	StaticMeshComp->SetIsReplicated(true);

	// SphereTrigger
	SphereTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("SphereTrigger"));
	SphereTrigger->SetSphereRadius(TriggerRange, true);
	SphereTrigger->SetupAttachment(StaticMeshComp);
	SphereTrigger->SetRelativeLocation(FVector(40.0f, 60.0f, 0.0f));
	SphereTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	SphereTrigger->SetIsReplicated(true);

	//SphereTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSLabyrinthKeyAltar::OnComponentBeginOverlapCallback);
	//SphereTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSLabyrinthKeyAltar::OnComponentEndOverlapCallback);

	InteractionPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptComponent"));
	InteractionPromptComponent->SetupAttachment(SphereTrigger);
	InteractionPromptComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	InteractionPromptComponent->SetVisibility(false);

	RequiredKeyCount = 5;
}

void ACSLabyrinthKeyAltar::BeginPlay()
{
	Super::BeginPlay();

	if (!StaticMesh.IsValid())
	{
		StaticMesh.LoadSynchronous();
	}

	if (StaticMesh.IsValid())
	{
		StaticMeshComp->SetStaticMesh(StaticMesh.Get());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("StaticMesh failed to load in ACSLabyrinthKey"));
	}

	if (!InteractionPromptWidgetClass.IsValid())
	{
		InteractionPromptWidgetClass.LoadSynchronous();
	}

	if (InteractionPromptWidgetClass.IsValid())
	{
		InteractionPromptComponent->SetWidgetClass(InteractionPromptWidgetClass.Get());
		InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
		InteractionPromptComponent->SetDrawSize(FVector2D(500.0f, 30.f));
		InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("InteractionPromptWidgetClass failed to load in ACSLabyrinthKeyAltar"));
	}
}

void ACSLabyrinthKeyAltar::BeginInteraction()
{
	InteractionPromptComponent->SetVisibility(true);
}

void ACSLabyrinthKeyAltar::EndInteraction()
{
	InteractionPromptComponent->SetVisibility(false);
}

void ACSLabyrinthKeyAltar::Interact()
{
	UCSLabyrinthKeyWorldSubsystem* LabyrinthKeySubsystem = GetWorld()->GetSubsystem<UCSLabyrinthKeyWorldSubsystem>();

	if (LabyrinthKeySubsystem)
	{
		int NowKeyCount = LabyrinthKeySubsystem->GetLabyrinthKeyCount();
		
		if ( NowKeyCount >= RequiredKeyCount )
		{
			ChangeLevel();
		}
		else
		{
			UE_LOG(LogCS, Log, TEXT("Needs More Key"));
		}
	}
}

void ACSLabyrinthKeyAltar::ChangeLevel()
{
	if ( HasAuthority() && GetWorld() )
	{
		GetWorld()->ServerTravel(TEXT("BaseMap?listen"));
	}
}
