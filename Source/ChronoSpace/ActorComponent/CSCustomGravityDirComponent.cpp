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

	// (removed 2026-08-23: 여기서 OrgGravityDirection 을 (0,0,-1) 로 리셋하던 줄 - 캐릭터 하나가 BeginPlay 할 때마다
	//  (리스폰, 분신 스폰) 월드 전체 중력 스위치 상태가 되돌아갔다. 레벨 시작 시 1회 리셋은 ACSGameMode::BeginPlay 가 한다.)

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
	
	// BlueprintSpawnableComponent 라 ACharacter 가 아닌 액터에 붙을 수 있다
	if ( !OwnerCharacter || !OwnerCharacter->GetCharacterMovement() ) return;

	if ( OwnerCharacter->HasAuthority() )
	{
		CheckGravity();
	}
	else
	{
		if ( TargetGravityDirection.IsNearlyZero() ) return;

		FVector CurrentDir = OwnerCharacter->GetCharacterMovement()->GetGravityDirection().GetSafeNormal();

		// 방향 벡터는 선형 보간(VInterpTo + 정규화)이 아니라 회전 보간으로.
		// 선형 보간은 정반대 방향(천장->바닥)에서 정규화하면 제자리라 한 틱도 진행하지 않았고,
		// DeltaTime*Speed == 0.5 인 프레임엔 영벡터가 되어 SetGravityDirection 이 ensure 를 때렸다.
		const float RotationRateDeg = GravityInterpSpeed * 90.0f;	// InterpSpeed 5 -> 450 deg/s (반전에 0.4초)
		FVector SmoothedDir = FMath::VInterpNormalRotationTo(CurrentDir, TargetGravityDirection, DeltaTime, RotationRateDeg);

		if ( !SmoothedDir.IsNearlyZero() && !CurrentDir.Equals(SmoothedDir, KINDA_SMALL_NUMBER) )
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
		UE_LOG(LogCS, Log, TEXT("OnActorBeginOverlapCallback: %s, %s"), *Core->Owner.GetName(), *GetOwner()->GetName());
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
		if (Character && Character->GetCharacterMovement())
		{
			Character->GetCharacterMovement()->SetGravityDirection(OrgGravityDirection);
		}
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
		const FVector NewDir = GetDirection();
		if (NewDir.IsNearlyZero()) return;	// 코어와 같은 위치면 방향이 없다 - 이전 값 유지 (SetGravityDirection(0) 은 ensure)

		CurrentGravityDirection = NewDir;
		OwnerCharacter->GetCharacterMovement()->SetGravityDirection(CurrentGravityDirection);
	}
}
