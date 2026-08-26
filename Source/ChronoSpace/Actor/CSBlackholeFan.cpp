// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSBlackholeFan.h"

#include "ChronoSpace.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACSBlackholeFan::ACSBlackholeFan()
{
	// 부모는 틱을 끄지만 팬은 매 프레임 각도를 다시 계산해야 한다.
	PrimaryActorTick.bCanEverTick = true;

	// 회전은 각 머신이 복제된 구간 정보에서 직접 구한다.
	// 트랜스폼까지 복제하면 계산 결과와 서로 덮어써서 떨린다.
	SetReplicateMovement(false);
}

void ACSBlackholeFan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSBlackholeFan, SpinPhase);
}

void ACSBlackholeFan::BeginPlay()
{
	Super::BeginPlay();

	InitialRotation = GetActorQuat();

	// 멈출 때 꺼야 하는 충돌 컴포넌트를 모아 두고, 되돌릴 값도 같이 기억한다.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (IsValid(Primitive) && Primitive->ComponentHasTag(BlockingComponentTag))
		{
			FCSFanBlocker Blocker;
			Blocker.Component = Primitive;
			Blocker.OriginalCollisionEnabled = Primitive->GetCollisionEnabled();
			Blockers.Add(Blocker);
		}
	}

	if (Blockers.Num() == 0)
	{
		UE_LOG(LogCS, Verbose, TEXT("[BlackholeFan] %s : '%s' 태그가 달린 컴포넌트가 없다. 충돌 토글은 하지 않는다."),
			*GetName(), *BlockingComponentTag.ToString());
	}

	// 시작 상태는 서버/클라가 같은 값으로 세팅되므로 복제를 기다릴 필요가 없다.
	// StartServerTime 이 0 이라 각도는 RotationSpeed * 서버시간 이 된다 (ACSRotatingActor 와 같은 식).
	SpinPhase.StartServerTime = 0.f;
	SpinPhase.StartAngle = 0.f;
	SpinPhase.StartSpeed = RotationSpeed;
	SpinPhase.TargetSpeed = RotationSpeed;
	SpinPhase.RampTime = 0.f;

	ApplyBlockingCollision(true);
}

float ACSBlackholeFan::GetSynchronizedWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.f;
}

float ACSBlackholeFan::EvaluateSpeed(float ServerTime) const
{
	const float Elapsed = FMath::Max(ServerTime - SpinPhase.StartServerTime, 0.f);

	if (SpinPhase.RampTime > UE_KINDA_SMALL_NUMBER && Elapsed < SpinPhase.RampTime)
	{
		return FMath::Lerp(SpinPhase.StartSpeed, SpinPhase.TargetSpeed, Elapsed / SpinPhase.RampTime);
	}

	return SpinPhase.TargetSpeed;
}

float ACSBlackholeFan::EvaluateAngle(float ServerTime) const
{
	const float Elapsed = FMath::Max(ServerTime - SpinPhase.StartServerTime, 0.f);

	// 램프 구간 안이면 선형 속도의 적분(사다리꼴)을 그대로 쓴다.
	if (SpinPhase.RampTime > UE_KINDA_SMALL_NUMBER && Elapsed < SpinPhase.RampTime)
	{
		return SpinPhase.StartAngle
			+ SpinPhase.StartSpeed * Elapsed
			+ (SpinPhase.TargetSpeed - SpinPhase.StartSpeed) * Elapsed * Elapsed / (2.f * SpinPhase.RampTime);
	}

	// 램프가 끝났으면 램프 전체 면적 + 남은 시간 x 목표 속도.
	const float RampAngle = (SpinPhase.StartSpeed + SpinPhase.TargetSpeed) * 0.5f * SpinPhase.RampTime;
	return SpinPhase.StartAngle + RampAngle + SpinPhase.TargetSpeed * (Elapsed - SpinPhase.RampTime);
}

float ACSBlackholeFan::GetCurrentSpinSpeed() const
{
	return EvaluateSpeed(GetSynchronizedWorldTime());
}

void ACSBlackholeFan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Angle = EvaluateAngle(GetSynchronizedWorldTime());

	// 로컬 축 회전이라 배치 회전에 곱한다. 성분끼리 더하면 축이 틀어진다.
	SetActorRotation(InitialRotation * FQuat(SpinAxis * Angle));
}

void ACSBlackholeFan::HandleActivationChanged()
{
	// 블루프린트의 OnActivated / OnDeactivated 는 그대로 나간다.
	Super::HandleActivationChanged();

	// 구간은 서버가 정하고 복제로 내려간다. 클라이언트는 OnRep 만 받는다.
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ResumeTimer);

	if (IsActivated())
	{
		// 블랙홀 감지 - 서서히 멈춘다.
		BeginSpinPhase(0.f, SpinDownTime);
		return;
	}

	// 감지 해제 - 곧바로 돌지 않고 잠깐 기다린다.
	if (ResumeDelay > UE_KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(ResumeTimer, this, &ACSBlackholeFan::StartSpinUp, ResumeDelay, false);
	}
	else
	{
		StartSpinUp();
	}
}

void ACSBlackholeFan::StartSpinUp()
{
	if (HasAuthority())
	{
		BeginSpinPhase(RotationSpeed, SpinUpTime);
	}
}

void ACSBlackholeFan::BeginSpinPhase(float NewTargetSpeed, float RampTime)
{
	const float Now = GetSynchronizedWorldTime();

	FCSFanSpinPhase NewPhase;
	NewPhase.StartServerTime = Now;
	// 각도는 360 으로 접어 둔다. 오래 돌아도 float 정밀도가 뭉개지지 않는다.
	NewPhase.StartAngle = FMath::Fmod(EvaluateAngle(Now), 360.f);
	NewPhase.StartSpeed = EvaluateSpeed(Now);
	NewPhase.TargetSpeed = NewTargetSpeed;
	NewPhase.RampTime = FMath::Max(RampTime, 0.f);

	SpinPhase = NewPhase;

	// OnRep 은 서버 자신에게 오지 않으므로 여기서 직접 부른다.
	OnRep_SpinPhase();
}

void ACSBlackholeFan::OnRep_SpinPhase()
{
	// 충돌은 복제된 구간에서 그대로 유도한다.
	// 클라이언트가 따로 타이머를 돌리지 않으므로 서버와 켜고 끄는 시점이 어긋나지 않는다.
	ApplyBlockingCollision(SpinPhase.TargetSpeed > UE_KINDA_SMALL_NUMBER);
}

void ACSBlackholeFan::ApplyBlockingCollision(bool bEnabled)
{
	for (const FCSFanBlocker& Blocker : Blockers)
	{
		if (UPrimitiveComponent* Primitive = Blocker.Component)
		{
			Primitive->SetCollisionEnabled(
				bEnabled ? Blocker.OriginalCollisionEnabled.GetValue() : ECollisionEnabled::NoCollision);
		}
	}
}
