// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_ProjectileBlackHole.h"
#include "GA/CSGA_BlackHole.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Character/CSCharacterPlayer.h"
#include "Actor/CSBlackHoleDummy.h"
#include "Actor/CSBlackHole.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "ChronoSpace.h"

UCSGA_ProjectileBlackHole::UCSGA_ProjectileBlackHole()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	GuideDuration = 5.0f;
	MaxGuideDistance = 2000.0f;
	UpdateRate = 0.02f;
	MouseYSensitivity = 3.0f;

	CurrentEndLocation = FVector::ZeroVector;

	//bIsDummySpawned = false;
	bIsBlackHoleSpawned = false;
	bIsAming = false;

	Duration = -1.0f;	// 블랙홀 지속 시간을 주고 싶으면 양수로
	GravityInfluenceRange = 500.0f;
	PullStrength = 10.0f;
	StopRange = 100.0f;

	bRetriggerInstancedAbility = true;
}

void UCSGA_ProjectileBlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	check(BlackHoleDummyClass);

	// GAS 컴포넌트 구조 상 서버는 이미 눌렸을 때 ActivateAbility 발동 안함
	// 클라 토글용 코드
	if ( bIsAming && !bIsBlackHoleSpawned )
	{
		bIsAming = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bIsAming = true;

	//bIsDummySpawned = false;

	if (!GetAvatarActorFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 초기 상태 설정
	bUsingMouseAiming = false;
	LastMousePosition = FVector2D::ZeroVector;
	bInitialDirectionSet = false;

	// 초기 조준 방향 저장 (한 번만 설정)
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		InitialAimDirection = Character->GetActorForwardVector();
		bInitialDirectionSet = true;
		UE_LOG(LogCS, Log, TEXT("Initial aim direction set: %s"), *InitialAimDirection.ToString());

		// 초기 마우스 위치 저장
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			float MouseX, MouseY;
			if (PC->GetMousePosition(MouseX, MouseY))
			{
				LastMousePosition = FVector2D(MouseX, MouseY);
			}
		}

		/*if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(Character))
		{
			CSPlayer->ZoomCamera( ZoomLength, ZoomSpeed );
		}*/
	}

	// 업데이트 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		UpdateTimerHandle,
		this,
		&UCSGA_ProjectileBlackHole::UpdateGuideLine,
		UpdateRate,
		true
	);

	UE_LOG(LogCS, Log, TEXT("ProjectileGuide Activated"));

}

FVector UCSGA_ProjectileBlackHole::GetScreenCenterDirection() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			// Determine which split-screen slot this player is in:
			// 0 = left/top, 1 = right/bottom (for two players)
			int32 ControllerId = PC->GetLocalPlayer()->GetControllerId();

			// Left player uses 25% X, right player 75%
			float ScreenCenterX = (ControllerId == 0)
				? ViewportSizeX * 0.25f
				: ViewportSizeX * 0.75f;

			float ScreenCenterY = ViewportSizeY * 0.5f;

			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				// apply Y-axis sensitivity as before
				float MouseYOffset = CurrentMouseY - ScreenCenterY;
				float AmplifiedYOffset = MouseYOffset * MouseYSensitivity;
				float FinalY = ScreenCenterY + AmplifiedYOffset;

				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectScreenPositionToWorld(ScreenCenterX, FinalY, WorldLocation, WorldDirection))
				{
					return WorldDirection;
				}
			}
		}
	}

	return bInitialDirectionSet ? InitialAimDirection : FVector::ForwardVector;
}

void UCSGA_ProjectileBlackHole::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if ( BlackHoleDummyActor )
	{
		BlackHoleDummyActor->Destroy();
	}

	/*if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor))
	{
		CSPlayer->ZoomCamera( 0, ZoomSpeed );
	}*/

	// 타이머 정리
	if (UpdateTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	if (DurationTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	UE_LOG(LogCS, Log, TEXT("ProjectileGuide Ended"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_ProjectileBlackHole::UpdateGuideLine()
{
	// 마우스 이동 감지
	CheckMouseMovement();

	FVector StartLocation = GetStartLocation();
	CurrentDirection = GetScreenCenterDirection();

	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection = FVector::ForwardVector;
	}

	// 화면 중앙 방향으로 라인 트레이스
	FVector EndLocation = StartLocation + CurrentDirection * MaxGuideDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	if ( IsValid( GetWorld() ) )
	{
		if ( UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem<UCSManagedActorSubsystem>(); Subsystem )
		{
			QueryParams.AddIgnoredActors( Subsystem->GetActorsPulledByBlackHole() );
		}
	}

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		EndLocation = HitResult.Location;
	}

	CurrentEndLocation = EndLocation;

	/*if( !bIsDummySpawned )
	{
		SpawnBlackHoleDummy(CurrentEndLocation);
		bIsDummySpawned = true;
	}

	if ( BlackHoleDummyActor )
	{
		BlackHoleDummyActor->SetActorLocation(CurrentEndLocation);

		if ( bIsBlackHoleSpawned )
		{
			BlackHoleDummyActor->Destroy(); 
		}
	}*/

	CheckMouseInput();
}

void UCSGA_ProjectileBlackHole::OnGuideDurationEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}


void UCSGA_ProjectileBlackHole::CheckMouseInput()
{
	if (ACSCharacterPlayer* Character = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			if ( PC->IsInputKeyDown(EKeys::RightMouseButton) )
			{
				if (!bIsBlackHoleSpawned)
				{
					CreateBlackHoleAtLocation(CurrentDirection);
					bIsBlackHoleSpawned = true;
				}
				else if (IsValid(Character->BlackHole))
				{
					if (Character->BlackHole->HasAuthority())
					{
						Character->BlackHole->SetActorLocation(CurrentEndLocation);
					}
					else
					{
						Character->ServerSetBlackHoleLocation(CurrentDirection, MaxGuideDistance);
					}
				}
			}
			else if (bIsBlackHoleSpawned)
			{
				Character->ServerDestoryBlackHole();
				bIsBlackHoleSpawned = false;
				bIsAming = false;

				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}

			/*if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
			{
				if ( !bIsBlackHoleSpawned )
				{
					CreateBlackHoleAtLocation(CurrentDirection);
					Character->ZoomCamera( 0, ZoomSpeed );
					bIsBlackHoleSpawned = true;
				}
				else if( IsValid(Character->BlackHole) )
				{
					if ( Character->BlackHole->HasAuthority() )
					{
						Character->BlackHole->SetActorLocation(CurrentEndLocation);
					}
					else
					{
						Character->ServerSetBlackHoleLocation(CurrentDirection, MaxGuideDistance);
					}
				}
			}
			else if(bIsBlackHoleSpawned)
			{
				Character->ServerDestoryBlackHole();
				bIsBlackHoleSpawned = false;
				bIsAming = false;

				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}*/
		}
	}
}

void UCSGA_ProjectileBlackHole::CreateBlackHoleAtLocation(const FVector& Direction)
{
	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo());

	if ( CSPlayer )
	{
		CSPlayer->ServerSpawnAndSetBlackHole(BlackHoleClass, Direction, MaxGuideDistance, Duration, GravityInfluenceRange, PullStrength, StopRange, bCheckMeshComponentPulledByBlackHole);
		bIsBlackHoleSpawned = true;
	}
}
/*
void UCSGA_ProjectileBlackHole::SpawnBlackHoleDummy(FVector SpawnLocation)
{
	FActorSpawnParameters Params;
	Params.Owner = GetOwningActorFromActorInfo();
	Params.Instigator = Cast<APawn>(GetAvatarActorFromActorInfo());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BlackHoleDummyActor =
		GetWorld()->SpawnActor<ACSBlackHoleDummy>(BlackHoleDummyClass, SpawnLocation,
			FRotator::ZeroRotator, Params);
	if (BlackHoleDummyActor)
	{
		BlackHoleDummyActor->SetGravityInfluenceRange( GravityInfluenceRange );
	}
}
*/
void UCSGA_ProjectileBlackHole::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// GAS 컴포넌트 구조상 서버에서만 불린다
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if ( Character == nullptr ) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if ( PC == nullptr ) return;

	// 이미 소환한 후에는 왼쪽 버튼 놔줄 때로 종료 체크
	if ( !bIsBlackHoleSpawned )
	{
		bIsAming = false;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}


FVector UCSGA_ProjectileBlackHole::GetStartLocation() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetActorLocation() + FVector(0.0f, 0.0f, Character->BaseEyeHeight);
	}

	return FVector::ZeroVector;
}

void UCSGA_ProjectileBlackHole::CheckMouseMovement()
{
	if (bUsingMouseAiming) return; // 이미 마우스 모드면 체크하지 않음

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				FVector2D CurrentMousePosition(CurrentMouseX, CurrentMouseY);

				// 마우스가 임계값 이상 움직였는지 확인
				float MouseDistance = FVector2D::Distance(LastMousePosition, CurrentMousePosition);

				if (MouseDistance > MouseMovementThreshold)
				{
					bUsingMouseAiming = true;
					UE_LOG(LogCS, Log, TEXT("Switched to mouse aiming mode (Distance: %f)"), MouseDistance);
				}

				LastMousePosition = CurrentMousePosition;
			}
		}
	}
}
