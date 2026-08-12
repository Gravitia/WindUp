// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSSplineTrack.h"
#include "Actor/CSSplineRider.h"
#include "Components/SplineComponent.h"
#include "Interface/CSReactorTarget.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

ACSSplineTrack::ACSSplineTrack()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
}

void ACSSplineTrack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSSplineTrack, ActivePointsMask);
}

void ACSSplineTrack::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerPoints.Num() > 32)
	{
		UE_LOG(LogCS, Warning, TEXT("[SplineTrack] %s : TriggerPoints 가 32개를 넘음. 33번째부터는 동작하지 않는다."), *GetName());
	}

	if (!HasAuthority()) return;

	// 인터페이스 미구현 타겟은 미리 경고해서 배치 실수를 잡는다. (리액터와 동일)
	for (const FCSSplineTriggerPoint& Entry : TriggerPoints)
	{
		for (AActor* Target : Entry.TargetActors)
		{
			if (Target && !Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
			{
				UE_LOG(LogCS, Warning, TEXT("[SplineTrack] %s : Target %s 이 ICSReactorTarget 을 구현하지 않음"),
					*GetName(), *Target->GetName());
			}
		}
	}
}

float ACSSplineTrack::GetDistanceAtPoint(int32 PointIndex) const
{
	if (!Spline) return 0.0f;

	const int32 ClampedIndex = FMath::Clamp(PointIndex, 0, Spline->GetNumberOfSplinePoints() - 1);
	return Spline->GetDistanceAlongSplineAtSplinePoint(ClampedIndex);
}

void ACSSplineTrack::RegisterRider(ACSSplineRider* Rider)
{
	if (Rider)
	{
		Riders.AddUnique(Rider);
	}
}

void ACSSplineTrack::UnregisterRider(ACSSplineRider* Rider)
{
	Riders.Remove(Rider);
}

float ACSSplineTrack::ResolveRiderMove(ACSSplineRider* Mover, float DesiredDistance)
{
	if (!Spline || !Mover) return DesiredDistance;

	const float SplineLength = Spline->GetSplineLength();
	DesiredDistance = FMath::Clamp(DesiredDistance, 0.0f, SplineLength);

	const float CurrentDistance = Mover->GetRailDistance();
	const float MoveDir = DesiredDistance - CurrentDistance;
	if (FMath::IsNearlyZero(MoveDir)) return DesiredDistance;

	const bool bForward = MoveDir > 0.0f;

	// 이동 방향에서 가장 가까운 라이더를 찾는다.
	ACSSplineRider* Blocking = nullptr;
	float BlockingDistance = 0.0f;
	float MinGap = 0.0f;

	for (auto It = Riders.CreateIterator(); It; ++It)
	{
		ACSSplineRider* Other = It->Get();
		if (!IsValid(Other))
		{
			It.RemoveCurrent();
			continue;
		}
		if (Other == Mover) continue;

		const float OtherDistance = Other->GetRailDistance();
		const float Gap = Mover->GetRailBlockRadius() + Other->GetRailBlockRadius();

		// 이동 방향 앞쪽에 있고, 목표 지점이 상대의 점유 범위를 침범하는가.
		const bool bInPath = bForward
			? (OtherDistance >= CurrentDistance && DesiredDistance > OtherDistance - Gap)
			: (OtherDistance <= CurrentDistance && DesiredDistance < OtherDistance + Gap);
		if (!bInPath) continue;

		if (!Blocking || (bForward ? OtherDistance < BlockingDistance : OtherDistance > BlockingDistance))
		{
			Blocking = Other;
			BlockingDistance = OtherDistance;
			MinGap = Gap;
		}
	}

	if (!Blocking) return DesiredDistance;

	// 고정(Lock) 중인 라이더는 밀리지 않고, 우선순위가 낮아도 막힌다.
	if (Blocking->IsRailLocked() || Mover->GetPushPriority() < Blocking->GetPushPriority())
	{
		return bForward ? (BlockingDistance - MinGap) : (BlockingDistance + MinGap);
	}

	// 밀어내기: 상대를 연쇄적으로 밀고(재귀), 상대가 실제로 멈춘 지점 뒤에 붙는다.
	const float PushTarget = bForward ? (DesiredDistance + MinGap) : (DesiredDistance - MinGap);
	const float OtherFinal = ResolveRiderMove(Blocking, PushTarget);
	Blocking->ApplyPushedDistance(OtherFinal, 0.0f);

	return bForward ? (OtherFinal - MinGap) : (OtherFinal + MinGap);
}

void ACSSplineTrack::NotifyRiderReachedPoint(int32 TriggerEntryIndex)
{
	if (!HasAuthority()) return;
	if (!TriggerPoints.IsValidIndex(TriggerEntryIndex) || TriggerEntryIndex >= 32) return;

	const int32 Bit = 1 << TriggerEntryIndex;
	if (ActivePointsMask & Bit) return;

	ActivePointsMask |= Bit;   // 서버에서 값 변경 → 클라로 리플리케이트
	HandleMaskChanged();
}

void ACSSplineTrack::NotifyRiderLeftPoint(int32 TriggerEntryIndex)
{
	if (!HasAuthority()) return;
	if (!TriggerPoints.IsValidIndex(TriggerEntryIndex) || TriggerEntryIndex >= 32) return;

	// Latch 성격의 포인트는 떠나도 유지.
	if (!TriggerPoints[TriggerEntryIndex].bDeactivateOnLeave) return;

	const int32 Bit = 1 << TriggerEntryIndex;
	if (!(ActivePointsMask & Bit)) return;

	ActivePointsMask &= ~Bit;
	HandleMaskChanged();
}

void ACSSplineTrack::OnRep_ActivePointsMask()
{
	HandleMaskChanged();
}

void ACSSplineTrack::HandleMaskChanged()
{
	const int32 Changed = ActivePointsMask ^ NotifiedMask;
	NotifiedMask = ActivePointsMask;

	for (int32 i = 0; i < TriggerPoints.Num() && i < 32; ++i)
	{
		const int32 Bit = 1 << i;
		if (!(Changed & Bit)) continue;

		const bool bActive = (ActivePointsMask & Bit) != 0;
		UE_LOG(LogCS, Log, TEXT("[SplineTrack] %s : TriggerPoint %d (SplinePoint %d) %s"),
			*GetName(), i, TriggerPoints[i].PointIndex, bActive ? TEXT("ACTIVATED") : TEXT("deactivated"));

		NotifyTargets(TriggerPoints[i].TargetActors, bActive);
	}
}

void ACSSplineTrack::NotifyTargets(const TArray<TObjectPtr<AActor>>& Targets, bool bActive)
{
	for (AActor* Target : Targets)
	{
		if (IsValid(Target) && Target->GetClass()->ImplementsInterface(UCSReactorTarget::StaticClass()))
		{
			ICSReactorTarget::Execute_OnReactorTriggerChanged(Target, bActive);
		}
	}
}
