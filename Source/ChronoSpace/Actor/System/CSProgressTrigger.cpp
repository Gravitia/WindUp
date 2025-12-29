// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSProgressTrigger.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UI/CSProgressUIWidget.h"

// Sets default values
ACSProgressTrigger::ACSProgressTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(Root);
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

// Called when the game starts or when spawned
void ACSProgressTrigger::BeginPlay()
{
	Super::BeginPlay();
	

	TriggerBox->OnComponentBeginOverlap.AddDynamic(
		this, &ACSProgressTrigger::OnTriggerBegin
	);
}


void ACSProgressTrigger::OnTriggerBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bTriggered)
		return;

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
		return;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
		return;

	if (!ProgressUIWidgetClass)
		return;

	UCSProgressUIWidget* Widget =
		CreateWidget<UCSProgressUIWidget>(PC, ProgressUIWidgetClass);

	if (!Widget)
		return;

	Widget->AddToViewport(50);
	Widget->Show(ProgressText, DisplayDuration, bProgressText);

	bTriggered = true;
	SetActorEnableCollision(false);
}

