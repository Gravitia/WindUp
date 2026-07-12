// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSReactorGroup.h"
#include "Actor/CSAbilityReactorBase.h"
#include "Interface/CSReactorTarget.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

ACSReactorGroup::ACSReactorGroup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// 그룹이 플레이어와 멀어도 상태 리플리케이션이 누락되지 않도록
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACSReactorGroup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSReactorGroup, bGroupActive);
}

void ACSReactorGroup::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	for (ACSAbilityReactorBase* Reactor : Reactors)
	{
		if (Reactor)
		{
			Reactor->OnReactorActivationChanged.AddUObject(this, &ACSReactorGroup::HandleReactorActivationChanged);
		}
	}

	// 인터페이스 미구현 타겟은 미리 경고해서 배치 실수를 잡는다.
	for (AActor* Target : TargetActors)
	{
		if (Target && !Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
		{
			UE_LOG(LogCS, Warning, TEXT("[ReactorGroup] %s : Target %s 이 ICSReactorTarget 을 구현하지 않음"),
				*GetName(), *Target->GetName());
		}
	}

	// 이미 켜져 있는(Latch 등) 리액터가 있을 수 있으니 초기 1회 평가.
	EvaluateGroup();
}

void ACSReactorGroup::HandleReactorActivationChanged(ACSAbilityReactorBase* /*Reactor*/, bool /*bActivated*/)
{
	EvaluateGroup();
}

void ACSReactorGroup::EvaluateGroup(bool bIgnoreLatch)
{
	if (!HasAuthority())
	{
		return;
	}

	int32 ValidCount = 0;
	int32 ActiveCount = 0;
	for (const ACSAbilityReactorBase* Reactor : Reactors)
	{
		if (Reactor)
		{
			++ValidCount;
			if (Reactor->IsActivated())
			{
				++ActiveCount;
			}
		}
	}

	bool bDesired = false;
	if (ValidCount > 0)
	{
		bDesired = (Logic == ECSReactorGroupLogic::All) ? (ActiveCount == ValidCount) : (ActiveCount > 0);
	}

	if (bLatch && !bIgnoreLatch)
	{
		bDesired = bGroupActive || bDesired;
	}

	SetGroupActive(bDesired);
}

void ACSReactorGroup::SetGroupActive(bool bNewActive)
{
	if (bGroupActive == bNewActive)
	{
		return;
	}

	bGroupActive = bNewActive;   // 서버에서 값 변경 → 클라로 리플리케이트

	// RepNotify 는 서버 자신에겐 호출되지 않으므로 서버에서 직접 실행한다.
	HandleGroupChanged();
}

void ACSReactorGroup::OnRep_GroupActive()
{
	HandleGroupChanged();
}

void ACSReactorGroup::HandleGroupChanged()
{
	if (bGroupDebug)
	{
		UE_LOG(LogCS, Log, TEXT("[ReactorGroup] %s : %s"), *GetName(), bGroupActive ? TEXT("ACTIVATED") : TEXT("deactivated"));
	}

	if (bGroupActive)
	{
		OnGroupActivated();
	}
	else
	{
		OnGroupDeactivated();
	}

	NotifyTargets(bGroupActive);
}

void ACSReactorGroup::NotifyTargets(bool bActive)
{
	for (AActor* Target : TargetActors)
	{
		if (IsValid(Target) && Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
		{
			ICSReactorTarget::Execute_OnReactorTriggerChanged(Target, bActive);
		}
	}
}

void ACSReactorGroup::ResetGroup(bool bAlsoResetReactors)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bAlsoResetReactors)
	{
		for (ACSAbilityReactorBase* Reactor : Reactors)
		{
			if (Reactor)
			{
				Reactor->ResetReactor();
			}
		}
	}

	// 래치를 무시하고 현재 리액터 상태 그대로 재평가.
	EvaluateGroup(true);
}
