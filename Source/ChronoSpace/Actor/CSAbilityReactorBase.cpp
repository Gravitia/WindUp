// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSAbilityReactorBase.h"
#include "Components/SphereComponent.h"
#include "Interface/CSAbilitySource.h"
#include "Interface/CSReactorTarget.h"
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
	DOREPLIFETIME(ACSAbilityReactorBase, bTriggerActive);
}

void ACSAbilityReactorBase::BeginPlay()
{
	Super::BeginPlay();

	// 부모 클래스가 만든 네이티브 컴포넌트라 정상적으로는 항상 있다.
	// 다만 예전에 같은 이름의 BP 컴포넌트를 쓰던 액터가 레벨에 배치돼 있으면
	// 저장된 인스턴스 데이터와 클래스가 어긋나 null 로 들어올 수 있다.
	// 그 경우 크래시 대신 로그로 알린다 - 해당 레벨을 다시 저장하면 정리된다.
	if (!Trigger)
	{
		UE_LOG(LogCS, Error,
			TEXT("[Reactor] %s : Trigger 컴포넌트가 없다. 레벨에 배치된 인스턴스 데이터가 옛 컴포넌트를 가리키고 있을 수 있다 - 레벨을 다시 저장할 것."),
			*GetName());
		return;
	}

	Trigger->SetSphereRadius(TriggerRadius, true);

	if (!HasAuthority()) return;

	// [타겟 트리거] 링크된 리액터들의 상태 변화를 구독한다.
	for (ACSAbilityReactorBase* Linked : LinkedReactors)
	{
		if (Linked && Linked != this)
		{
			Linked->OnReactorActivationChanged.AddUObject(this, &ACSAbilityReactorBase::HandleLinkedReactorChanged);
		}
	}

	// 인터페이스 미구현 타겟은 미리 경고해서 배치 실수를 잡는다.
	for (AActor* Target : TargetActors)
	{
		if (Target && !Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
		{
			UE_LOG(LogCS, Warning, TEXT("[Reactor] %s : Target %s 이 ICSReactorTarget 을 구현하지 않음"),
				*GetName(), *Target->GetName());
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
	ApplyActivationFromConditions();
}

void ACSAbilityReactorBase::OnReactorTriggerChanged_Implementation(bool bActive)
{
	// 그룹이 서버/클라 양쪽에서 타겟을 호출하지만, 리액터 상태는 서버에서만 바꾼다.
	// (클라이언트는 bIsActivated 리플리케이션으로 따라온다)
	if (!HasAuthority()) return;

	bool bNewExternal = bActive;

	// Latch(1회): 외부 트리거도 한번 켜지면 유지.
	if (Mode == ECSReactorMode::Latch)
	{
		bNewExternal = bExternalCondition || bNewExternal;
	}

	if (bNewExternal == bExternalCondition) return;

	bExternalCondition = bNewExternal;
	ApplyActivationFromConditions();
}

void ACSAbilityReactorBase::ApplyActivationFromConditions()
{
	// 능력 오버랩(bLocalCondition)과 외부 트리거(bExternalCondition) 중 하나라도 참이면 활성.
	SetActivated(bLocalCondition || bExternalCondition);
}

void ACSAbilityReactorBase::SetActivated(bool bNewActivated)
{
	if (bIsActivated == bNewActivated) return;

	bIsActivated = bNewActivated;   // 서버에서 값 변경 → 클라로 리플리케이트

	// RepNotify 는 서버 자신에겐 호출되지 않으므로 서버에서 직접 반응을 실행한다.
	HandleActivationChanged();

	// 나를 링크한 대표 리액터 등 구독자에게 통지. (서버 전용)
	OnReactorActivationChanged.Broadcast(this, bIsActivated);

	// 내 상태가 바뀌면 내 조합(타겟 트리거)도 재평가.
	EvaluateTrigger();
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
	bExternalCondition = false;

	SetActivated(false);

	// 조합 래치도 무시하고 현재 상태 그대로 재평가.
	EvaluateTrigger(true);
}

void ACSAbilityReactorBase::HandleLinkedReactorChanged(ACSAbilityReactorBase* /*Reactor*/, bool /*bActivated*/)
{
	EvaluateTrigger();
}

void ACSAbilityReactorBase::EvaluateTrigger(bool bIgnoreLatch)
{
	if (!HasAuthority()) return;

	// 통지할 타겟이 없는 리액터는 조합 판정도 하지 않는다. (감지 전용)
	if (TargetActors.Num() == 0) return;

	int32 ValidCount = 1;
	int32 ActiveCount = bIsActivated ? 1 : 0;
	for (const ACSAbilityReactorBase* Linked : LinkedReactors)
	{
		if (Linked && Linked != this)
		{
			++ValidCount;
			if (Linked->IsActivated())
			{
				++ActiveCount;
			}
		}
	}

	bool bDesired = (TriggerLogic == ECSReactorGroupLogic::All) ? (ActiveCount == ValidCount) : (ActiveCount > 0);

	if (bTriggerLatch && !bIgnoreLatch)
	{
		bDesired = bTriggerActive || bDesired;
	}

	SetTriggerActive(bDesired);
}

void ACSAbilityReactorBase::SetTriggerActive(bool bNewActive)
{
	if (bTriggerActive == bNewActive) return;

	bTriggerActive = bNewActive;   // 서버에서 값 변경 → 클라로 리플리케이트

	// RepNotify 는 서버 자신에겐 호출되지 않으므로 서버에서 직접 실행한다.
	HandleTriggerStateChanged();
}

void ACSAbilityReactorBase::OnRep_TriggerActive()
{
	HandleTriggerStateChanged();
}

void ACSAbilityReactorBase::HandleTriggerStateChanged()
{
	if (bReactorDebug)
	{
		UE_LOG(LogCS, Log, TEXT("[Reactor %s] Trigger %s"), *GetName(), bTriggerActive ? TEXT("ACTIVATED") : TEXT("deactivated"));
	}

	NotifyTargets(bTriggerActive);
}

void ACSAbilityReactorBase::NotifyTargets(bool bActive)
{
	for (AActor* Target : TargetActors)
	{
		if (IsValid(Target) && Target != this && Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
		{
			ICSReactorTarget::Execute_OnReactorTriggerChanged(Target, bActive);
		}
	}
}
