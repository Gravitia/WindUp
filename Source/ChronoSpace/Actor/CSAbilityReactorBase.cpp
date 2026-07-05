// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSAbilityReactorBase.h"
#include "Components/SphereComponent.h"
#include "Interface/CSAbilitySource.h"
#include "Physics/CSCollision.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "ChronoSpace.h"

ACSAbilityReactorBase::ACSAbilityReactorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetSphereRadius(TriggerRadius, true);
	Trigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	Trigger->SetIsReplicated(true);
	RootComponent = Trigger;

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACSAbilityReactorBase::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ACSAbilityReactorBase::OnTriggerEndOverlap);
}

void ACSAbilityReactorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSAbilityReactorBase, bIsActivated);
}

void ACSAbilityReactorBase::BeginPlay()
{
	Super::BeginPlay();

	Trigger->SetSphereRadius(TriggerRadius, true);

	// 짝을 한쪽에만 지정해도 서로 연결되도록 서버에서 역참조를 보정한다.
	if (HasAuthority() && PairedReactor && PairedReactor->PairedReactor == nullptr)
	{
		PairedReactor->PairedReactor = this;

		// 짝 로직도 지정 안 된 쪽에만 맞춰준다(기획자가 한쪽만 세팅해도 대칭 동작).
		if (PairedReactor->PairLogic == ECSReactorPairLogic::None)
		{
			PairedReactor->PairLogic = PairLogic;
		}
	}
}

void ACSAbilityReactorBase::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepHitResult*/)
{
	if (!HasAuthority()) return;

	const bool bInterested = IsAbilitySourceOfInterest(OtherActor);

	if (bReactorDebug)
	{
		const ICSAbilitySource* Src = Cast<ICSAbilitySource>(OtherActor);
		const FString Msg = FString::Printf(TEXT("[Reactor %s] Begin Other=%s IsSource=%d Type=%d RespondsTo=%d -> %s"),
			*GetName(), *GetNameSafe(OtherActor), Src ? 1 : 0,
			Src ? static_cast<int32>(Src->GetAbilityType()) : -1,
			RespondsToAbilities, bInterested ? TEXT("MATCH") : TEXT("ignore"));
		UE_LOG(LogCS, Log, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, bInterested ? FColor::Green : FColor::Silver, Msg);
	}

	if (!bInterested) return;

	++OverlapRefCount;
	RecomputeLocalCondition();
}

void ACSAbilityReactorBase::OnTriggerEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority()) return;
	if (!IsAbilitySourceOfInterest(OtherActor)) return;

	OverlapRefCount = FMath::Max(0, OverlapRefCount - 1);
	RecomputeLocalCondition();
}

bool ACSAbilityReactorBase::IsAbilitySourceOfInterest(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor)) return false;

	const ICSAbilitySource* Source = Cast<ICSAbilitySource>(OtherActor);
	if (Source == nullptr) return false;

	// 아무 능력도 체크하지 않았으면(0) 모든 능력 소스에 반응.
	if (RespondsToAbilities == 0) return true;

	// enum 값은 비트 인덱스 → (1 << 값)으로 마스크를 만들어 비교.
	const int32 SourceMask = 1 << static_cast<int32>(Source->GetAbilityType());
	return (RespondsToAbilities & SourceMask) != 0;
}

void ACSAbilityReactorBase::RecomputeLocalCondition()
{
	bool bNewLocal = (OverlapRefCount > 0);

	// Latch(1회): 한번 켜지면 유지. EndOverlap 으로 꺼지지 않는다.
	if (Mode == ECSReactorMode::Latch)
	{
		bNewLocal = bLocalCondition || bNewLocal;
	}

	if (bNewLocal == bLocalCondition) return;

	bLocalCondition = bNewLocal;

	// 내 조건이 바뀌면 나와 짝 둘 다 최종 활성 여부를 다시 계산한다.
	EvaluateActivation();
	if (PairedReactor)
	{
		PairedReactor->EvaluateActivation();
	}
}

void ACSAbilityReactorBase::EvaluateActivation()
{
	SetActivated(ComputeDesiredActivation());
}

bool ACSAbilityReactorBase::ComputeDesiredActivation() const
{
	switch (PairLogic)
	{
	case ECSReactorPairLogic::RequiresBoth:
		return bLocalCondition && (PairedReactor != nullptr && PairedReactor->bLocalCondition);

	case ECSReactorPairLogic::Either:
		return bLocalCondition || (PairedReactor != nullptr && PairedReactor->bLocalCondition);

	case ECSReactorPairLogic::None:
	default:
		return bLocalCondition;
	}
}

void ACSAbilityReactorBase::SetActivated(bool bNewActivated)
{
	if (bIsActivated == bNewActivated) return;

	bIsActivated = bNewActivated;   // 서버에서 값 변경 → 클라로 리플리케이트

	// RepNotify 는 서버 자신에겐 호출되지 않으므로 서버에서 직접 반응을 실행한다.
	HandleActivationChanged();
}

void ACSAbilityReactorBase::OnRep_IsActivated()
{
	HandleActivationChanged();
}

void ACSAbilityReactorBase::HandleActivationChanged()
{
	if (bIsActivated)
	{
		OnActivated();
	}
	else
	{
		OnDeactivated();
	}
}

void ACSAbilityReactorBase::ResetReactor()
{
	if (!HasAuthority()) return;

	OverlapRefCount = 0;
	bLocalCondition = false;

	EvaluateActivation();
	if (PairedReactor)
	{
		PairedReactor->EvaluateActivation();
	}
}
