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

	bIsSpawned = false;

	Duration = 5.0f;
	GravityInfluenceRange = 500.0f;
	PullStrength = 10.0f;
	StopRange = 100.0f;
}

void UCSGA_ProjectileBlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	check(BlackHoleDummyClass);

	bIsSpawned = false;

	if (!GetAvatarActorFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 초기 상태 설정
	bUsingMouseAiming = false;
	LastMousePosition = FVector2D::ZeroVector;
	bInitialDirectionSet = false;

	// ★ 초기 조준 방향 저장 (한 번만 설정)
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

		if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(Character))
		{
			CSPlayer->SetShoulderLook(false);
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

	// 지속시간 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		DurationTimerHandle,
		this,
		&UCSGA_ProjectileBlackHole::OnGuideDurationEnd,
		GuideDuration,
		false
	);

	UE_LOG(LogCS, Log, TEXT("ProjectileGuide Activated"));

}

FVector UCSGA_ProjectileBlackHole::GetScreenCenterDirection() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			// ★ 마우스 조준 모드가 아니면 저장된 초기 방향 사용 (플레이어 회전에 영향받지 않음)
			//if (!bUsingMouseAiming)
			//{
			//	if (bInitialDirectionSet)
			//	{
			//		return InitialAimDirection;
			//	}
			//	else
			//	{
			//		// 백업: 현재 방향 사용
			//		return Character->GetActorForwardVector();
			//	}
			//}

			// 마우스 조준 모드일 때만 마우스 위치 사용
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				float ScreenCenterX = ViewportSizeX * 0.5f;
				float ScreenCenterY = ViewportSizeY * 0.5f;

				// 마우스 Y축 중앙에서 차이 계산
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

	// Fallback: 저장된 초기 방향 또는 ForwardVector
	return bInitialDirectionSet ? InitialAimDirection : FVector::ForwardVector;
}

void UCSGA_ProjectileBlackHole::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if ( BlackHoleDummyActor )
	{
		BlackHoleDummyActor->Destroy();
	}

	if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor))
	{
		CSPlayer->SetShoulderLook(true);
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

	UE_LOG(LogCS, Log, TEXT("ProjectileGuide Ended"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_ProjectileBlackHole::UpdateGuideLine()
{
	// 마우스 이동 감지
	CheckMouseMovement();

	FVector StartLocation = GetStartLocation();
	FVector ScreenCenterDirection = GetScreenCenterDirection();

	if (ScreenCenterDirection.IsNearlyZero())
	{
		ScreenCenterDirection = FVector::ForwardVector;
	}

	// 화면 중앙 방향으로 라인 트레이스
	FVector EndLocation = StartLocation + ScreenCenterDirection * MaxGuideDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		EndLocation = HitResult.Location;
	}

	CurrentEndLocation = EndLocation;

	// 조준 모드에 따라 다른 색상으로 표시
	FColor SphereColor = bUsingMouseAiming ? FColor::Red : FColor::Orange;
	DrawDebugSphere(GetWorld(), EndLocation, 45.0f, 8, SphereColor, false, UpdateRate + 0.01f);

	if( !bIsSpawned )
	{
		SpawnBlackHoleDummy(CurrentEndLocation);
		bIsSpawned = true;
	}

	if ( BlackHoleDummyActor )
	{
		UE_LOG(LogCS, Log, TEXT("BlackHoleDummyActor: %f, %f, %f"), 
			BlackHoleDummyActor->GetActorLocation().X,
			BlackHoleDummyActor->GetActorLocation().Y,
			BlackHoleDummyActor->GetActorLocation().Z);
		BlackHoleDummyActor->SetActorLocation(CurrentEndLocation);
	}
	

	CheckMouseInput();
}

void UCSGA_ProjectileBlackHole::OnGuideDurationEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}


void UCSGA_ProjectileBlackHole::CheckMouseInput()
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			// 왼쪽 마우스 버튼 클릭 감지
			if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
			{
				UE_LOG(LogCS, Log, TEXT("Left mouse clicked! Creating BlackHole at: %s"), *CurrentEndLocation.ToString());
				CreateBlackHoleAtLocation(CurrentEndLocation);

				// 블랙홀 생성 후 가이드 어빌리티 종료
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}
	}
}

void UCSGA_ProjectileBlackHole::CreateBlackHoleAtLocation(const FVector& Location)
{
	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo());

	if ( CSPlayer )
	{
		CSPlayer->ServerSpawnAndSetBlackHole(BlackHoleClass, Location, Duration, GravityInfluenceRange, PullStrength, StopRange);
	}
}

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
