// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSCustomGravityDirComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Actor/CSGravityCore.h"
#include "Engine/World.h"
#include "ChronoSpace.h"

FVector UCSCustomGravityDirComponent::OrgGravityDirection = FVector(0.0f, 0.0f, -1.0f);


UCSCustomGravityDirComponent::UCSCustomGravityDirComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	GravityInterpSpeed = 5.0f;

	SetIsReplicatedByDefault(true);
}


void UCSCustomGravityDirComponent::BeginPlay()
{
	Super::BeginPlay();
	UCSCustomGravityDirComponent::OrgGravityDirection = FVector(0.0f, 0.0f, -1.0f);

	if ( GetOwner() )
	{
		OwnerCharacter = Cast<ACharacter>(GetOwner());

		if ( OwnerCharacter && OwnerCharacter->HasAuthority() )
		{
			//UE_LOG(LogCS, Log, TEXT("[NetMode : %d] BeginPlay"), GetNetMode());
			OwnerCharacter->OnActorBeginOverlap.AddDynamic(this, &UCSCustomGravityDirComponent::OnActorBeginOverlapCallback);
			OwnerCharacter->OnActorEndOverlap.AddDynamic(this, &UCSCustomGravityDirComponent::OnActorEndOverlapCallback);
		}
	}
}

void UCSCustomGravityDirComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//UE_LOG(LogCS, Log, TEXT("TickComponent"));
	if ( OwnerCharacter->HasAuthority() )
	{
		CheckGravity();
	}
	else
	{
		FVector CurrentDir = OwnerCharacter->GetCharacterMovement()->GetGravityDirection().GetSafeNormal();

		FVector SmoothedDir = FMath::VInterpTo(CurrentDir, TargetGravityDirection, DeltaTime, GravityInterpSpeed).GetSafeNormal();

		if ( !CurrentDir.Equals(SmoothedDir, KINDA_SMALL_NUMBER) )
		{
			OwnerCharacter->GetCharacterMovement()->SetGravityDirection(SmoothedDir);
		}
	}
	
}

void UCSCustomGravityDirComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCSCustomGravityDirComponent, CurrentGravityDirection);
}

FVector UCSCustomGravityDirComponent::GetDirection()
{
	if ( CurrentGravityCore )
	{
		FVector CoreLocation = CurrentGravityCore->GetActorLocation();
		FVector CharacterLocation = OwnerCharacter->GetActorLocation();

		return (CoreLocation - CharacterLocation).GetSafeNormal();
	}

	return FVector();
} 

void UCSCustomGravityDirComponent::OnRep_CurrentGravityDirection()
{
	TargetGravityDirection = CurrentGravityDirection.GetSafeNormal();
}

void UCSCustomGravityDirComponent::OnActorBeginOverlapCallback(AActor* OverlappedActor, AActor* OtherActor)
{
	ACSGravityCore* Core = Cast<ACSGravityCore>(OtherActor);

	if ( Core )
	{
		UE_LOG(LogCS, Log, TEXT("[Netmode %d] UCSCustomGravityDirComponent OnActorBeginOverlapCallback"), GetWorld()->GetNetMode());
		CurrentGravityCore = Core;
	}
}

void UCSCustomGravityDirComponent::OnActorEndOverlapCallback(AActor* OverlappedActor, AActor* OtherActor)
{
	ACSGravityCore* Core = Cast<ACSGravityCore>(OtherActor);
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	if (Core)
	{
		CurrentGravityDirection = OrgGravityDirection;
		Character->GetCharacterMovement()->SetGravityDirection(OrgGravityDirection);
		CurrentGravityCore = nullptr;
	}
}

void UCSCustomGravityDirComponent::CheckGravity()
{
	if (OwnerCharacter == nullptr) return;

	if (OwnerCharacter->HasAuthority() && CurrentGravityCore)
	{
		CurrentGravityDirection = GetDirection();
		OwnerCharacter->GetCharacterMovement()->SetGravityDirection(CurrentGravityDirection);
	}
}
