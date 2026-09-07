// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_ProjectileBlackHole.h"
#include "GA/CSGA_BlackHole.h"
#include "GA/CSGA_CameraZoom.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Character/CSCharacterPlayer.h"
#include "ActorComponent/CSCameraRigComponent.h"
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

	CameraZOffsetWhileAiming = 400.0f;
	bApplyCameraZOffsetWhileAiming = true;

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
	}

	// 업데이트 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		UpdateTimerHandle,
		this,
		&UCSGA_ProjectileBlackHole::UpdateGuideLine,
		UpdateRate,
		true
	);
	/*
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (CameraZoomAbilityClass)
		{
			ASC->TryActivateAbilityByClass(CameraZoomAbilityClass);
		}
	}
	*/

	ApplyCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Activated"));

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

			// Left player uses 75% X (right side), right player 25% X (left side)
			float ScreenCenterX = (ControllerId == 0)
				? ViewportSizeX * 0.75f
				: ViewportSizeX * 0.25f;

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

	// 타이머 정리
	if (UpdateTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	if (DurationTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	// 카메라 복원은 캐릭터의 UCSCameraRigComponent 가 Tick 으로 돌린다.
	// 어빌리티 타이머로 돌리면 바로 아래 Super::EndAbility 안의
	// ClearAllTimersForObject(this) 에 지워져서 복원이 한 번도 실행되지 않는다.
	RestoreCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Ended"));
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

				/*
				if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
				{
					if (CameraZoomAbilityClass)
					{
						if (FGameplayAbilitySpec* Spec =
							ASC->FindAbilitySpecFromClass(CameraZoomAbilityClass))
						{
							ASC->CancelAbilityHandle(Spec->Handle);
						}
					}
				}
				*/

				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
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

		/*
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (CameraZoomAbilityClass)
			{
				if (FGameplayAbilitySpec* Spec =
					ASC->FindAbilitySpecFromClass(CameraZoomAbilityClass))
				{
					ASC->CancelAbilityHandle(Spec->Handle);
				}
			}
		}
		*/

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


ACSCharacterPlayer* UCSGA_ProjectileBlackHole::GetCameraRigOwner(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* Avatar = (ActorInfo && ActorInfo->AvatarActor.IsValid())
		? ActorInfo->AvatarActor.Get()
		: GetAvatarActorFromActorInfo();

	return Cast<ACSCharacterPlayer>(Avatar);
}

void UCSGA_ProjectileBlackHole::ApplyCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bApplyCameraZOffsetWhileAiming)
	{
		return;
	}

	ACSCharacterPlayer* CSPlayer = GetCameraRigOwner(ActorInfo);
	if (!IsValid(CSPlayer))
	{
		UE_LOG(LogCS, Warning, TEXT("ApplyCameraZOffset: avatar is not ACSCharacterPlayer"));
		return;
	}

	FCSCameraModifier Modifier;
	Modifier.Source = CSCameraRigSource::BlackHoleAim;
	Modifier.BoomOffsetDelta = FVector(0.f, 0.f, CameraZOffsetWhileAiming);
	Modifier.BlendInTime = CameraOffsetLerpDuration;
	Modifier.BlendOutTime = CameraOffsetRestoreLerpDuration;
	Modifier.Blend = ECSCameraBlend::EaseInOut;

	CSPlayer->AddCameraModifier(Modifier);

	UE_LOG(LogCS, Log, TEXT("ApplyCameraZOffset: Z + %f over %f seconds"), CameraZOffsetWhileAiming, CameraOffsetLerpDuration);
}

void UCSGA_ProjectileBlackHole::RestoreCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo)
{
	ACSCharacterPlayer* CSPlayer = GetCameraRigOwner(ActorInfo);
	if (!IsValid(CSPlayer))
	{
		// 여기로 빠지면 오프셋이 걸린 채 남는다. 흔적을 남겨야 추적이 된다
		UE_LOG(LogCS, Warning, TEXT("RestoreCameraZOffset: avatar is not ACSCharacterPlayer - BlackHoleAim offset left active"));
		return;
	}

	CSPlayer->RemoveCameraModifier(CSCameraRigSource::BlackHoleAim);

	UE_LOG(LogCS, Log, TEXT("RestoreCameraZOffset: release over %f seconds"), CameraOffsetRestoreLerpDuration);
}
