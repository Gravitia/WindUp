#include "GA/CSGA_ProjectileGuide.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ChronoSpace.h"

UCSGA_ProjectileGuide::UCSGA_ProjectileGuide()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	GuideDuration = 5.0f;
	MaxGuideDistance = 2000.0f;
	UpdateRate = 0.02f;
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
	FVector PlayerDirection = GetPlayerForwardDirection();

	if (PlayerDirection.IsNearlyZero())
	{
		PlayerDirection = FVector::ForwardVector;
	}

	// 라인 트레이스
	FVector EndLocation = StartLocation + PlayerDirection * MaxGuideDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		EndLocation = HitResult.Location;
	}

	// 간단한 디버그 라인으로 가이드 표시
	DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red, false, UpdateRate + 0.01f, 0, 3.0f);
	DrawDebugSphere(GetWorld(), EndLocation, 15.0f, 8, FColor::Orange, false, UpdateRate + 0.01f);
}

void UCSGA_ProjectileGuide::OnGuideDurationEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FVector UCSGA_ProjectileGuide::GetPlayerForwardDirection() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		// 플레이어 컨트롤러의 회전값 사용 (마우스 시점 반영)
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			return PC->GetControlRotation().Vector();
		}

		// Fallback: 캐릭터의 Forward 방향
		return Character->GetActorForwardVector();
	}

	return FVector::ForwardVector;
}

FVector UCSGA_ProjectileGuide::GetStartLocation() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		// 플레이어 눈 높이에서 시작
		return Character->GetActorLocation() + FVector(0, 0, Character->BaseEyeHeight);
	}

	return FVector::ZeroVector;
}