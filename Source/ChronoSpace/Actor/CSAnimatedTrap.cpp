// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSAnimatedTrap.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

ACSAnimatedTrap::ACSAnimatedTrap()
{
	PrimaryActorTick.bCanEverTick = true;
}

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::BeginPlay()
{
	Super::BeginPlay();

	ResolveTargetComponents();

	if (bAutoStart && HasAuthority())
	{
		StartAnim();
	}
}

void ACSAnimatedTrap::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSAnimatedTrap, bAnimActive);
	DOREPLIFETIME(ACSAnimatedTrap, ServerStartTime);
}

/* ─────────────────────────────────────────────
 *  Tick
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAnimActive)
	{
		ApplyAnimation();
	}
}

/* ─────────────────────────────────────────────
 *  Replication
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::OnRep_AnimState()
{
	if (!bAnimActive)
	{
		// 정지 시 모든 컴포넌트 초기 위치로 복원
		for (FAnimTrapEntry& Entry : AnimEntries)
		{
			if (Entry.CachedComp)
			{
				Entry.CachedComp->SetRelativeLocation(Entry.InitialLocation);
				Entry.CachedComp->SetRelativeRotation(Entry.InitialRotation);
			}
		}
	}
}

/* ─────────────────────────────────────────────
 *  Control API
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::StartAnim()
{
	if (!HasAuthority()) return;

	bAnimActive     = true;

	float TimeToUse = GetWorld()->GetTimeSeconds();
	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		TimeToUse = GameState->GetServerWorldTimeSeconds();
	}
	ServerStartTime = TimeToUse;

	ForceNetUpdate();
}

void ACSAnimatedTrap::StopAnim()
{
	if (!HasAuthority()) return;

	bAnimActive = false;
	ForceNetUpdate();

	for (FAnimTrapEntry& Entry : AnimEntries)
	{
		if (Entry.CachedComp)
		{
			Entry.CachedComp->SetRelativeLocation(Entry.InitialLocation);
			Entry.CachedComp->SetRelativeRotation(Entry.InitialRotation);
		}
	}
}

/* ─────────────────────────────────────────────
 *  Animation Core — 모든 엔트리에 대해 적용
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::ApplyAnimation()
{
	float WorldTime = GetWorld()->GetTimeSeconds();
	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		WorldTime = GameState->GetServerWorldTimeSeconds();
	}
	const float Elapsed   = WorldTime - ServerStartTime;

	for (FAnimTrapEntry& Entry : AnimEntries)
	{
		if (!Entry.CachedComp || Entry.Period <= 0.0f) continue;

		const float Alpha = CalcAlpha(Elapsed, Entry.Period, Entry.HoldTime);

		if (Entry.AnimType == EAnimTrapType::Translate)
		{
			const FVector Offset = GetAxisVector(Entry.AnimAxis) * Alpha * Entry.Amplitude;
			Entry.CachedComp->SetRelativeLocation(Entry.InitialLocation + Offset);
		}
		else // Rotate
		{
			const FRotator Offset = MakeAxisRotator(Entry.AnimAxis, Alpha * Entry.Amplitude);
			Entry.CachedComp->SetRelativeRotation(Entry.InitialRotation + Offset);
		}
	}
}

/* ─────────────────────────────────────────────
 *  Helpers
 * ───────────────────────────────────────────── */

void ACSAnimatedTrap::ResolveTargetComponents()
{
	TArray<UActorComponent*> AllComps;
	GetComponents(AllComps);

	for (FAnimTrapEntry& Entry : AnimEntries)
	{
		if (Entry.TargetComponentTag.IsNone())
		{
			Entry.CachedComp = GetRootComponent();
		}
		else
		{
			for (UActorComponent* Comp : AllComps)
			{
				if (Comp && Comp->ComponentHasTag(Entry.TargetComponentTag))
				{
					Entry.CachedComp = Cast<USceneComponent>(Comp);
					if (Entry.CachedComp) break;
				}
			}

			if (!Entry.CachedComp)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[CSAnimatedTrap] '%s': Tag '%s' not found. Skipping entry."),
					*GetName(), *Entry.TargetComponentTag.ToString());
				continue;
			}
		}

		if (Entry.CachedComp)
		{
			Entry.InitialLocation = Entry.CachedComp->GetRelativeLocation();
			Entry.InitialRotation = Entry.CachedComp->GetRelativeRotation();
		}
	}
}

FVector ACSAnimatedTrap::GetAxisVector(EAnimTrapAxis Axis)
{
	switch (Axis)
	{
	case EAnimTrapAxis::X: return FVector::ForwardVector;
	case EAnimTrapAxis::Y: return FVector::RightVector;
	case EAnimTrapAxis::Z: return FVector::UpVector;
	default:               return FVector::UpVector;
	}
}

FRotator ACSAnimatedTrap::MakeAxisRotator(EAnimTrapAxis Axis, float Deg)
{
	switch (Axis)
	{
	case EAnimTrapAxis::X: return FRotator(0.0f, 0.0f, Deg); // Roll
	case EAnimTrapAxis::Y: return FRotator(Deg, 0.0f, 0.0f); // Pitch
	case EAnimTrapAxis::Z: return FRotator(0.0f, Deg, 0.0f); // Yaw
	default:               return FRotator(0.0f, Deg, 0.0f);
	}
}

/**
 * HoldTime == 0 → 기존 sin 파형 (연속 진동)
 * HoldTime >  0 → 내려가기 → 대기 → 올라오기 (cos ease-in/out)
 *
 *  파형 (예: Amplitude 양수, 내려가는 방향):
 *   0  ___         ___
 *      |  \       /
 *      |   \     /
 *  -1  |    \___/
 *      |    hold
 *      +---time------->
 */
float ACSAnimatedTrap::CalcAlpha(float Elapsed, float Period, float HoldTime)
{
	if (HoldTime <= 0.0f)
	{
		// 기존 연속 sin 파형
		return FMath::Sin(2.0f * PI * Elapsed / Period);
	}

	const float HalfPeriod  = Period * 0.5f;
	const float TotalCycle  = Period + HoldTime;
	const float t           = FMath::Fmod(Elapsed, TotalCycle);

	if (t < HalfPeriod)
	{
		// 내려가기 (0 → -1, cos ease-out)
		return -0.5f * (1.0f - FMath::Cos(PI * t / HalfPeriod));
	}
	else if (t < HalfPeriod + HoldTime)
	{
		// 끝점 대기
		return -1.0f;
	}
	else
	{
		// 올라오기 (-1 → 0, cos ease-in)
		const float t2 = t - HalfPeriod - HoldTime;
		return -0.5f * (1.0f + FMath::Cos(PI * t2 / HalfPeriod));
	}
}
