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
		if (!Entry.CachedComp) continue;

		const float Alpha = CalcAlpha(Elapsed, Entry);

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
 *  Sine      : sin(2π·t/Period) 연속 진동
 *  HoldPause : 0 → -1 (cos ease-out) → hold → -1 → 0 (cos ease-in)
 *  Impulse   : 0 → -1 (cubic ease-out, 빠름) → hold → -1 → 0 (cos ease-in/out, 느림)
 *
 *  파형 (Amplitude 양수, 내려가는 방향):
 *
 *   HoldPause                Impulse
 *   0 ___        ___         0 |\__              ___
 *     |  \      /              | |  \           /
 *     |   \    /               | |   \         /
 *  -1 |    \__/             -1 | |    \_______/
 *         hold                  attack hold  release
 */
float ACSAnimatedTrap::CalcAlpha(float Elapsed, const FAnimTrapEntry& Entry)
{
	switch (Entry.WaveShape)
	{
	case EAnimTrapWave::Sine:
	{
		if (Entry.Period <= 0.0f) return 0.0f;
		return FMath::Sin(2.0f * PI * Elapsed / Entry.Period);
	}

	case EAnimTrapWave::HoldPause:
	{
		if (Entry.Period <= 0.0f) return 0.0f;

		const float HalfPeriod = Entry.Period * 0.5f;
		const float TotalCycle = Entry.Period + Entry.HoldTime;
		const float t          = FMath::Fmod(Elapsed, TotalCycle);

		if (t < HalfPeriod)
		{
			return -0.5f * (1.0f - FMath::Cos(PI * t / HalfPeriod));
		}
		else if (t < HalfPeriod + Entry.HoldTime)
		{
			return -1.0f;
		}
		else
		{
			const float t2 = t - HalfPeriod - Entry.HoldTime;
			return -0.5f * (1.0f + FMath::Cos(PI * t2 / HalfPeriod));
		}
	}

	case EAnimTrapWave::Impulse:
	{
		const float Snap       = FMath::Max(Entry.SnapTime,   KINDA_SMALL_NUMBER);
		const float Return     = FMath::Max(Entry.ReturnTime, KINDA_SMALL_NUMBER);
		const float TotalCycle = Snap + Entry.HoldTime + Return;
		const float t          = FMath::Fmod(Elapsed, TotalCycle);

		if (t < Snap)
		{
			// 0 → -1, cubic ease-out (빠르게 도착)
			const float u  = t / Snap;
			const float iu = 1.0f - u;
			return -(1.0f - iu * iu * iu);
		}
		else if (t < Snap + Entry.HoldTime)
		{
			return -1.0f;
		}
		else
		{
			// -1 → 0, cos ease-in/out (천천히 복귀)
			const float t2 = t - Snap - Entry.HoldTime;
			const float u  = t2 / Return;
			return -0.5f * (1.0f + FMath::Cos(PI * u));
		}
	}
	}

	return 0.0f;
}
