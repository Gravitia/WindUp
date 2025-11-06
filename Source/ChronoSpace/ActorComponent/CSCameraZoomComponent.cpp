// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSCameraZoomComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ChronoSpace.h"
#include "Player/CSPlayerController.h"

// Sets default values for this component's properties
UCSCameraZoomComponent::UCSCameraZoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UCSCameraZoomComponent::Init(USpringArmComponent* SpringArm, float InOrgLength)
{
	CameraBoom = SpringArm;
	OrgLength = InOrgLength;
	bIsInit = true;
}

void UCSCameraZoomComponent::ZoomCamera(float InZoomLength, float InZoomSpeed)
{
	if (!bIsInit) return;

	TargetZoom = InZoomLength;
	ZoomSpeed = InZoomSpeed;

	const float NewArmLength = (InZoomLength == 0) ? 1200.f : 600.f;   // will fix

	if (AActor* Owner = GetOwner())
	{
		if (APawn* Pawn = Cast<APawn>(Owner))
		{
			if (ACSPlayerController* PC = Cast<ACSPlayerController>(Pawn->GetController()))
			{
				// 반드시 로컬 컨트롤러에서만 호출해야 서버로 전송됨
				if (PC->IsLocalController())
				{
					PC->ServerBroadcastZoomToOthers(NewArmLength);
				}
			}
		}
	}
}
void UCSCameraZoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if ( !bIsInit || CameraBoom == nullptr) return;

	if ( FMath::Abs( CameraBoom->TargetArmLength - ( OrgLength + TargetZoom ) ) < KINDA_SMALL_NUMBER ) {
		if ( FMath::Abs( TargetZoom ) < KINDA_SMALL_NUMBER ) {
			CameraBoom->TargetArmLength = OrgLength;
		}

		return;
	}

	// Zoom In
	if ( TargetZoom >= 0 && CameraBoom->TargetArmLength > OrgLength - TargetZoom ) 
	{
		UE_LOG(LogCS, Log, TEXT("Zoom In"));
		CameraBoom->TargetArmLength -= ZoomSpeed * SpeedCoef * DeltaTime;

		if ( CameraBoom->TargetArmLength <= OrgLength - TargetZoom)
		{
			CameraBoom->TargetArmLength = OrgLength - TargetZoom;
		}
	}

	// Zoom Out
	if ( TargetZoom <= 0 && CameraBoom->TargetArmLength < OrgLength - TargetZoom)
	{
		UE_LOG(LogCS, Log, TEXT("Zoom Out"));
		CameraBoom->TargetArmLength += ZoomSpeed * SpeedCoef * DeltaTime;

		if (CameraBoom->TargetArmLength >= OrgLength - TargetZoom)
		{
			CameraBoom->TargetArmLength = OrgLength - TargetZoom;
		}
	}
}

