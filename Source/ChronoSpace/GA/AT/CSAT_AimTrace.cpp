// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/AT/CSAT_AimTrace.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Subsystem/CSManagedActorSubsystem.h"

UCSAT_AimTrace::UCSAT_AimTrace()
{
	bTickingTask = true;
}

UCSAT_AimTrace* UCSAT_AimTrace::CreateAimTraceTask(UGameplayAbility* OwningAbility, float InMaxDistance, float InMouseYSensitivity, float InUpdateInterval)
{
	UCSAT_AimTrace* Task = NewAbilityTask<UCSAT_AimTrace>(OwningAbility);
	Task->MaxDistance = InMaxDistance;
	Task->MouseYSensitivity = InMouseYSensitivity;
	Task->UpdateInterval = FMath::Max(InUpdateInterval, 0.f);
	return Task;
}

void UCSAT_AimTrace::Activate()
{
	Super::Activate();

	if (const AActor* Avatar = GetAvatarActor(); Avatar)
	{
		FallbackDirection = Avatar->GetActorForwardVector();
	}

	// 첫 갱신을 기다리지 않도록 즉시 한 번 흘려보낸다
	Accumulated = UpdateInterval;
}

void UCSAT_AimTrace::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	Accumulated += DeltaTime;
	if (Accumulated < UpdateInterval)
	{
		return;
	}
	// 0 으로 리셋하면 나머지가 버려져 60fps 에서 2프레임(33ms)마다 돈다. 빼기만 해야 평균 주기가 설정값과 맞는다.
	// 프레임이 크게 밀렸을 때 다음 프레임에 연달아 터지지 않도록 한 주기만 남긴다.
	Accumulated = FMath::Min(Accumulated - UpdateInterval, UpdateInterval);

	const AActor* Avatar = GetAvatarActor();
	UWorld* World = IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	FVector Direction = ComputeAimDirection(Avatar, World);
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	const FVector EndLocation = TraceAimEnd(Avatar, Direction, MaxDistance);

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnAimUpdated.Broadcast(Direction, EndLocation);
	}
}

FVector UCSAT_AimTrace::ComputeAimDirection(const AActor* Avatar, UWorld* World) const
{
	const ACharacter* Character = Cast<ACharacter>(Avatar);
	if (!Character)
	{
		return FallbackDirection;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP)
	{
		return FallbackDirection;
	}

	int32 ViewportSizeX = 0, ViewportSizeY = 0;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

	// "정면" 은 이 플레이어 뷰의 중심이다. UCSViewFamilyViewportClient 가 분할 중에 LocalPlayer 의
	// Origin/Size 를 메인 렉트로 좁혀 두므로(디프로젝션도 그걸 쓴다), 중심을 여기서 읽으면
	// 분할 좌/우, Split↔Fullscreen 전환 중, 풀스크린 전부 자동으로 맞는다.
	// (예전엔 ControllerId 로 75%/25% 를 하드코딩했는데, cs.SplitScreen.FixedSide 로 좌우가 바뀌면 틀어졌다.)
	const float ScreenCenterX = ViewportSizeX * (LP->Origin.X + LP->Size.X * 0.5f);
	const float ScreenCenterY = ViewportSizeY * (LP->Origin.Y + LP->Size.Y * 0.5f);

	float MouseX = 0.f, MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return FallbackDirection;
	}

	// Y 축만 감도를 곱해 중앙 기준으로 증폭한다
	const float FinalY = ScreenCenterY + (MouseY - ScreenCenterY) * MouseYSensitivity;

	FVector CamLocation, CamDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenCenterX, FinalY, CamLocation, CamDirection))
	{
		return FallbackDirection;
	}

	// 수렴 조준: 카메라 레이가 바라보는 지점을 구하고, 눈에서 그 지점을 향한다.
	// 카메라 방향을 눈에서 평행하게 쏘면 카메라와 눈의 위치 차이만큼 항상 어긋난다(시차).
	const FVector Eye = GetAimStartLocation(Avatar);

	// 카메라 뒤·옆 물체(캐릭터 뒤 벽 등)에 걸리지 않도록, 레이 위에서 눈과 나란한 지점부터 트레이스한다
	const float AlongToEye = FVector::DotProduct(Eye - CamLocation, CamDirection);
	const FVector TraceStart = CamLocation + CamDirection * FMath::Max(AlongToEye, 0.f);
	const FVector TraceEnd = TraceStart + CamDirection * MaxDistance;

	FHitResult Hit;
	const FVector LookPoint = LineTraceIgnoringAvatar(World, Avatar, TraceStart, TraceEnd, Hit) ? Hit.Location : TraceEnd;

	const FVector ToLookPoint = (LookPoint - Eye).GetSafeNormal();
	return ToLookPoint.IsNearlyZero() ? CamDirection : ToLookPoint;
}

bool UCSAT_AimTrace::LineTraceIgnoringAvatar(UWorld* World, const AActor* Avatar, const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	if (UCSManagedActorSubsystem* Subsystem = World->GetSubsystem<UCSManagedActorSubsystem>(); Subsystem)
	{
		QueryParams.AddIgnoredActors(Subsystem->GetActorsPulledByBlackHole());
	}

	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);
}

FVector UCSAT_AimTrace::GetAimStartLocation(const AActor* Avatar)
{
	if (const ACharacter* Character = Cast<ACharacter>(Avatar); Character)
	{
		return Character->GetActorLocation() + FVector(0.f, 0.f, Character->BaseEyeHeight);
	}
	return IsValid(Avatar) ? Avatar->GetActorLocation() : FVector::ZeroVector;
}

FVector UCSAT_AimTrace::TraceAimEnd(const AActor* Avatar, const FVector& Direction, float MaxDistance)
{
	// 아바타가 없으면 원점 기준 레이가 나온다. 호출자가 뭘 하든 원점은 답이 아니다.
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	const FVector StartLocation = GetAimStartLocation(Avatar);
	FVector EndLocation = StartLocation + Direction * MaxDistance;

	UWorld* World = Avatar->GetWorld();
	if (!IsValid(World))
	{
		return EndLocation;
	}

	FHitResult HitResult;
	if (LineTraceIgnoringAvatar(World, Avatar, StartLocation, EndLocation, HitResult))
	{
		EndLocation = HitResult.Location;
	}

	return EndLocation;
}
