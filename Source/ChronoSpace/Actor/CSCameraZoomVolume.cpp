// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSCameraZoomVolume.h"
#include "Components/BoxComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "ChronoSpace.h"

ACSCameraZoomVolume::ACSCameraZoomVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCameraZoomVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSCameraZoomVolume::OnTriggerEndOverlap);
}

void ACSCameraZoomVolume::BeginPlay()
{
	Super::BeginPlay();
}

void ACSCameraZoomVolume::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	if (!Player->IsLocallyControlled()) return;

	Player->ZoomCamera(ZoomLength, ZoomSpeed);

	UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: %s entered → ZoomLength: %.1f"), *Player->GetName(), ZoomLength);
}

void ACSCameraZoomVolume::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	if (!Player->IsLocallyControlled()) return;

	Player->ZoomCamera(0.f, ZoomSpeed);

	UE_LOG(LogCS, Log, TEXT("CameraZoomVolume: %s exited → Restore original zoom"), *Player->GetName());
}
