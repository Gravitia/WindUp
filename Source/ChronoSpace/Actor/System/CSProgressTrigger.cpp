// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSProgressTrigger.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UI/CSProgressUIWidget.h"
#include "Interface/CSProgressActivatable.h"

ACSProgressTrigger::ACSProgressTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	// 멀티에서 서버가 상태를 바꿀 거면, 트리거도 복제 켜두는 게 안정적
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(Root);
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

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
	if (bTriggerOnce && bTriggered)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	// =================
	// 1) UI는 "해당 플레이어 로컬"에서만 띄우기
	// =================
	if (Pawn->IsLocallyControlled())
	{
		APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
		if (PC && ProgressUIWidgetClass)
		{
			if (UCSProgressUIWidget* Widget = CreateWidget<UCSProgressUIWidget>(PC, ProgressUIWidgetClass))
			{
				Widget->AddToViewport(50);
				Widget->Show(ProgressText, DisplayDuration, bProgressText);
			}
		}
	}

	// =================
	// 2) 가동/정지는 서버에서만 처리
	// =================
	if (!HasAuthority())
	{
		return;
	}

	// Activate
	for (AActor* Target : ActivateTargets)
	{
		if (!IsValid(Target))
			continue;

		if (Target->GetClass()->ImplementsInterface(UCSProgressActivatable::StaticClass()))
		{
			ICSProgressActivatable::Execute_Activate(Target, Pawn, TriggerId);
		}
	}

	// Deactivate
	for (AActor* Target : DeactivateTargets)
	{
		if (!IsValid(Target))
			continue;

		if (Target->GetClass()->ImplementsInterface(UCSProgressActivatable::StaticClass()))
		{
			ICSProgressActivatable::Execute_Deactivate(Target, Pawn, TriggerId);
		}
	}

	// 트리거 상태 처리
	bTriggered = true;

	// 충돌 비활성(재발동 방지)
	SetActorEnableCollision(false);

	// 전파 빨리
	ForceNetUpdate();
}

