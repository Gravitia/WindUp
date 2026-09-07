// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSCameraRigComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ChronoSpace.h"

namespace CSCameraRig
{
	float BlendTimeFromSpeed(float Distance, float ZoomSpeed)
	{
		// 예전 UCSCameraZoomComponent::SpeedCoef 와 같은 값
		const float UnitsPerSecond = ZoomSpeed * 100.f;

		// 예전엔 델타가 0 이라 카메라가 아예 안 움직였다. 그 의미를 유지한다
		if (UnitsPerSecond <= KINDA_SMALL_NUMBER)
		{
			return -1.f;
		}

		return FMath::Abs(Distance) / UnitsPerSecond;
	}
}

UCSCameraRigComponent::UCSCameraRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UCSCameraRigComponent::Init(USpringArmComponent* SpringArm, float InOrgLength)
{
	CameraBoom = SpringArm;
	OrgLength = InOrgLength;
	BaseBoomOffset = SpringArm ? SpringArm->GetRelativeLocation() : FVector::ZeroVector;

	// 리스폰/재빙의로 다시 불릴 수 있다. 이전 수명의 모디파이어를 들고 가면
	// 새 Base 위에 옛 효과가 얹혀 카메라가 어긋난 채로 시작한다
	Modifiers.Reset();

	bIsInit = (SpringArm != nullptr);
}

// 값으로 받는다. 참조로 받으면 아래 AddDefaulted_GetRef 의 재할당에
// 호출자가 넘긴 참조가 Modifiers 원소일 때 dangling 이 된다
void UCSCameraRigComponent::AddModifier(FCSCameraModifier InModifier)
{
	if (!bIsInit || InModifier.Source.IsNone()) return;

	const FName Source = InModifier.Source;

	FCSCameraModifier* Modifier = Modifiers.FindByPredicate(
		[Source](const FCSCameraModifier& Entry) { return Entry.Source == Source; });

	if (Modifier == nullptr)
	{
		Modifier = &Modifiers.AddDefaulted_GetRef();
	}

	// 해제 블렌드 도중 다시 걸리는 경우 Alpha 를 리셋하지 않는다. 0 으로 되돌리면 카메라가 튄다
	const float KeepAlpha = Modifier->Alpha;
	*Modifier = InModifier;
	Modifier->Alpha = KeepAlpha;
	Modifier->bReleasing = false;
}

void UCSCameraRigComponent::RemoveModifier(FName Source)
{
	for (FCSCameraModifier& Modifier : Modifiers)
	{
		if (Modifier.Source == Source)
		{
			Modifier.bReleasing = true;
			return;
		}
	}
}

void UCSCameraRigComponent::ClearModifiers()
{
	Modifiers.Reset();
	ApplyToSpringArm();
}

void UCSCameraRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if ( !bIsInit || CameraBoom == nullptr ) return;

	// 모디파이어가 없으면 SpringArm 은 이미 Base 다. 매 프레임 쓰지 않는다
	if (Modifiers.Num() == 0) return;

	for (int32 Index = Modifiers.Num() - 1; Index >= 0; --Index)
	{
		FCSCameraModifier& Modifier = Modifiers[Index];

		const float BlendTime = Modifier.bReleasing ? Modifier.BlendOutTime : Modifier.BlendInTime;
		const float TargetAlpha = Modifier.bReleasing ? 0.f : 1.f;

		if (BlendTime > KINDA_SMALL_NUMBER)
		{
			const float Step = DeltaTime / BlendTime;

			Modifier.Alpha = (TargetAlpha > Modifier.Alpha)
				? FMath::Min(Modifier.Alpha + Step, TargetAlpha)
				: FMath::Max(Modifier.Alpha - Step, TargetAlpha);
		}
		else if (BlendTime >= 0.f || Modifier.bReleasing)
		{
			// 0 = 즉시. 해제 중이면 음수여도 즉시 처리한다 -
			// 여기서 얼려두면 아래 제거 조건에 영영 안 걸려 모디파이어가 남는다
			Modifier.Alpha = TargetAlpha;
		}
		// 적용 중 음수 = 움직이지 않음 (예전 ZoomSpeed 0). Alpha 를 건드리지 않는다

		if (Modifier.bReleasing && Modifier.Alpha <= 0.f)
		{
			Modifiers.RemoveAt(Index);
		}
	}

	// 마지막 모디파이어가 빠진 프레임에도 여기까지 온다 - 그 프레임에 Base 가 정확히 기록된다
	ApplyToSpringArm();
}

void UCSCameraRigComponent::ApplyToSpringArm()
{
	if (CameraBoom == nullptr) return;

	float ArmLength = OrgLength;
	FVector BoomOffset = BaseBoomOffset;

	for (const FCSCameraModifier& Modifier : Modifiers)
	{
		const float Weight = (Modifier.Blend == ECSCameraBlend::EaseInOut)
			? FMath::InterpEaseInOut(0.f, 1.f, Modifier.Alpha, 2.f)
			: Modifier.Alpha;

		ArmLength += Modifier.ArmLengthDelta * Weight;
		BoomOffset += Modifier.BoomOffsetDelta * Weight;
	}

	// 음수 팔 길이는 클램프하지 않는다. 엔진 SpringArm 의 정상 동작이고 예전에도 그대로 통과시켰다
	if (!FMath::IsNearlyEqual(CameraBoom->TargetArmLength, ArmLength))
	{
		CameraBoom->TargetArmLength = ArmLength;
	}

	if (!CameraBoom->GetRelativeLocation().Equals(BoomOffset))
	{
		CameraBoom->SetRelativeLocation(BoomOffset);
	}
}
