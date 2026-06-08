#include "Actor/CSTransformMover.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameStateBase.h"

ACSTransformMover::ACSTransformMover()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MovingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MovingRoot"));
	MovingRoot->SetupAttachment(SceneRoot);
}

void ACSTransformMover::BeginPlay()
{
	Super::BeginPlay();

	InitialMovingRootTransform = MovingRoot->GetComponentTransform();
}

void ACSTransformMover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!MovementState.bActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	ApplyMovement();

	const float Elapsed = GetSynchronizedWorldTime() - MovementState.ServerStartTime;
	if (HasAuthority() && Elapsed >= MovementState.Duration)
	{
		FinishMovement();
	}
}

bool ACSTransformMover::PlayMovement(FName MovementKey)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FCSTransformMovementPreset* Preset = MovementPresets.Find(MovementKey);
	if (!Preset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CSTransformMover] '%s': Movement key '%s' was not found."),
			*GetName(), *MovementKey.ToString());
		return false;
	}

	return StartMovement(MovementKey, MakeTargetTransform(*Preset), Preset->Duration);
}

bool ACSTransformMover::ReturnToInitial()
{
	if (!HasAuthority())
	{
		return false;
	}

	return StartMovement(NAME_None, InitialMovingRootTransform, ReturnDuration);
}

void ACSTransformMover::StopMovement(bool bSnapToTarget)
{
	if (!HasAuthority() || !MovementState.bActive)
	{
		return;
	}

	if (bSnapToTarget)
	{
		MovingRoot->SetWorldTransform(MovementState.TargetTransform);
	}

	MovementState.bActive = false;
	SetActorTickEnabled(false);
	MulticastStopMovement(MovementState.TargetTransform, bSnapToTarget);
}

void ACSTransformMover::MulticastStartMovement_Implementation(
	FName MovementKey,
	const FTransform& StartTransform,
	const FTransform& TargetTransform,
	float Duration)
{
	if (HasAuthority())
	{
		return;
	}

	MovementState.MovementKey = MovementKey;
	MovementState.StartTransform = StartTransform;
	MovementState.TargetTransform = TargetTransform;
	MovementState.ServerStartTime = GetSynchronizedWorldTime();
	MovementState.Duration = Duration;
	MovementState.bActive = true;

	MovingRoot->SetWorldTransform(StartTransform);
	SetActorTickEnabled(true);
	OnMovementStarted.Broadcast(MovementKey);
}

void ACSTransformMover::MulticastFinishMovement_Implementation(
	FName MovementKey,
	const FTransform& TargetTransform)
{
	if (HasAuthority())
	{
		return;
	}

	MovingRoot->SetWorldTransform(TargetTransform);
	MovementState.MovementKey = MovementKey;
	MovementState.TargetTransform = TargetTransform;
	MovementState.bActive = false;
	SetActorTickEnabled(false);
	OnMovementFinished.Broadcast(MovementKey);
}

void ACSTransformMover::MulticastStopMovement_Implementation(
	const FTransform& TargetTransform,
	bool bSnapToTarget)
{
	if (HasAuthority())
	{
		return;
	}

	if (bSnapToTarget)
	{
		MovingRoot->SetWorldTransform(TargetTransform);
	}

	MovementState.TargetTransform = TargetTransform;
	MovementState.bActive = false;
	SetActorTickEnabled(false);
}

bool ACSTransformMover::StartMovement(
	FName MovementKey,
	const FTransform& TargetTransform,
	float Duration)
{
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	MovementState.MovementKey = MovementKey;
	MovementState.StartTransform = MovingRoot->GetComponentTransform();
	MovementState.TargetTransform = TargetTransform;
	MovementState.ServerStartTime = GetSynchronizedWorldTime();
	MovementState.Duration = Duration;
	MovementState.bActive = true;

	MovingRoot->SetWorldTransform(MovementState.StartTransform);
	SetActorTickEnabled(true);
	OnMovementStarted.Broadcast(MovementKey);
	MulticastStartMovement(
		MovementKey,
		MovementState.StartTransform,
		MovementState.TargetTransform,
		MovementState.Duration);
	return true;
}

void ACSTransformMover::ApplyMovement()
{
	const float Elapsed = GetSynchronizedWorldTime() - MovementState.ServerStartTime;
	const float NormalizedTime = FMath::Clamp(Elapsed / MovementState.Duration, 0.0f, 1.0f);
	const float Alpha = EvaluateAlpha(NormalizedTime);

	FTransform NewTransform;
	NewTransform.Blend(
		MovementState.StartTransform,
		MovementState.TargetTransform,
		Alpha);
	MovingRoot->SetWorldTransform(NewTransform);
}

void ACSTransformMover::FinishMovement()
{
	MovingRoot->SetWorldTransform(MovementState.TargetTransform);
	MovementState.bActive = false;
	SetActorTickEnabled(false);

	OnMovementFinished.Broadcast(MovementState.MovementKey);
	MulticastFinishMovement(MovementState.MovementKey, MovementState.TargetTransform);
}

float ACSTransformMover::GetSynchronizedWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

float ACSTransformMover::EvaluateAlpha(float NormalizedTime) const
{
	const UCurveFloat* Curve = ReturnCurve;
	if (!MovementState.MovementKey.IsNone())
	{
		if (const FCSTransformMovementPreset* Preset = MovementPresets.Find(MovementState.MovementKey))
		{
			Curve = Preset->Curve;
		}
	}

	return Curve
		? FMath::Clamp(Curve->GetFloatValue(NormalizedTime), 0.0f, 1.0f)
		: NormalizedTime;
}

FTransform ACSTransformMover::MakeTargetTransform(const FCSTransformMovementPreset& Preset) const
{
	if (!Preset.bRelativeToInitial)
	{
		return Preset.TargetTransform;
	}

	return Preset.TargetTransform * InitialMovingRootTransform;
}
