#include "GA/CSGA_ProjectileGuide.h"
#include "GA/CSGA_BlackHole.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ChronoSpace.h"

UCSGA_ProjectileGuide::UCSGA_ProjectileGuide()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	GuideDuration = 5.0f;
	MaxGuideDistance = 2000.0f;
	UpdateRate = 0.02f;
	MouseYSensitivity = 3.0f;

	// 블랙홀 어빌리티 클래스 기본값 설정 (에디터에서 설정 가능)
	BlackHoleAbilityClass = UCSGA_BlackHole::StaticClass();

	CurrentEndLocation = FVector::ZeroVector;
}

void UCSGA_ProjectileGuide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!GetAvatarActorFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 업데이트 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		UpdateTimerHandle,
		this,
		&UCSGA_ProjectileGuide::UpdateGuideLine,
		UpdateRate,
		true
	);

	// 지속시간 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		DurationTimerHandle,
		this,
		&UCSGA_ProjectileGuide::OnGuideDurationEnd,
		GuideDuration,
		false
	);

	FixedStartLocation = GetStartLocation();

	UE_LOG(LogCS, Log, TEXT("ProjectileGuide Activated"));
}

void UCSGA_ProjectileGuide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
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

void UCSGA_ProjectileGuide::UpdateGuideLine()
{
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

	// 간단한 디버그 라인으로 가이드 표시
	//DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red, false, UpdateRate + 0.01f, 0, 3.0f);
	DrawDebugSphere(GetWorld(), EndLocation, 45.0f, 8, FColor::Orange, false, UpdateRate + 0.01f);

	CheckMouseInput();
}

void UCSGA_ProjectileGuide::OnGuideDurationEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FVector UCSGA_ProjectileGuide::GetScreenCenterDirection() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			// 화면 해상도 구하기
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			// 현재 마우스 위치 구하기
			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				// 화면 중앙 좌표 계산
				float ScreenCenterX = ViewportSizeX * 0.5f;
				float ScreenCenterY = ViewportSizeY * 0.5f;

				// 마우스 Y축 중앙에서 차이 계산
				float MouseYOffset = CurrentMouseY - ScreenCenterY;

				// 민감도 적용하여 차이 증폭
				float AmplifiedYOffset = MouseYOffset * MouseYSensitivity;

				// 최종 Y 좌표 계산
				float FinalY = ScreenCenterY + AmplifiedYOffset;

				// 화면 중앙을 월드 방향으로 변환
				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectScreenPositionToWorld(ScreenCenterX, FinalY, WorldLocation, WorldDirection))
				{
					return WorldDirection;
				}
			}
			else
			{
				// 마우스 위치를 가져올 수 없으면 기본 중앙에서 약간 위로
				float ScreenCenterX = ViewportSizeX * 0.5f;
				float ScreenCenterY = -100.0f + ViewportSizeY * 0.5f;

				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectScreenPositionToWorld(ScreenCenterX, ScreenCenterY, WorldLocation, WorldDirection))
				{
					return WorldDirection;
				}
			}
		}
	}

	// Fallback: 플레이어가 바라보는 방향
	return FVector::ForwardVector;
}

void UCSGA_ProjectileGuide::CheckMouseInput()
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

void UCSGA_ProjectileGuide::CreateBlackHoleAtLocation(const FVector& Location)
{
	if (!BlackHoleAbilityClass)
	{
		UE_LOG(LogCS, Warning, TEXT("BlackHoleAbilityClass is not set!"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		UE_LOG(LogCS, Warning, TEXT("AbilitySystemComponent not found via ActorInfo!"));
		return;
	}

	// 블랙홀 어빌리티에 위치 정보 설정
	UCSGA_BlackHole::SetPendingTargetLocation(Location);

	// 블랙홀 어빌리티 스펙 생성 및 활성화
	FGameplayAbilitySpec BlackHoleSpec(BlackHoleAbilityClass, 1, -1, this);
	FGameplayAbilitySpecHandle SpecHandle = ASC->GiveAbility(BlackHoleSpec);

	if (ASC->TryActivateAbility(SpecHandle, true))
	{
		UE_LOG(LogCS, Log, TEXT("BlackHole ability activated at location: %s"), *Location.ToString());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("Failed to activate BlackHole ability"));
	}
}



FVector UCSGA_ProjectileGuide::GetStartLocation() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetActorLocation() + FVector(0.0f, 0.0f, Character->BaseEyeHeight);
	}

	return FVector::ZeroVector;
}