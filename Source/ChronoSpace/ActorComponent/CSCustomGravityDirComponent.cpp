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

	bIsGravityCustomized = false;

	SetIsReplicatedByDefault(true);
}


void UCSCustomGravityDirComponent::BeginPlay()
{
	Super::BeginPlay();

	//SetComponentTickEnabled(false);

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
	DOREPLIFETIME(UCSCustomGravityDirComponent, bIsGravityCustomized);
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
		if (Core->Owner == GetOwner()) return;

		UE_LOG(LogCS, Log, TEXT("[Netmode %d] UCSCustomGravityDirComponent OnActorBeginOverlapCallback"), GetWorld()->GetNetMode());
		CurrentGravityCore = Core;
		
		//SetComponentTickEnabled(true);
		bIsGravityCustomized = true;
	}
}

void UCSCustomGravityDirComponent::OnActorEndOverlapCallback(AActor* OverlappedActor, AActor* OtherActor)
{
	ACSGravityCore* Core = Cast<ACSGravityCore>(OtherActor);
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	if (Core)
	{
		if (Core->Owner == GetOwner()) return;

		CurrentGravityDirection = OrgGravityDirection;
		Character->GetCharacterMovement()->SetGravityDirection(OrgGravityDirection);
		CurrentGravityCore = nullptr;

		//SetComponentTickEnabled(false);
		bIsGravityCustomized = false;
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
