// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorComponent/CSObjectResetComponent.h"

#include "ChronoSpace.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Subsystem/CSObjectResetSubsystem.h"
#include "TimerManager.h"

UCSObjectResetComponent::UCSObjectResetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 순수하게 범위를 나타내는 도형이다. 충돌에 끼면 오브젝트 물리가 이상해진다.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);

	// 생성자에서는 SetBoxExtent 를 쓰면 안 된다.
	// UpdateBodySetup() 이 NewObject 를 부르는데, CDO 생성 중에는 이름 없는 NewObject 가 금지라 죽는다.
	// InitBoxExtent 는 값만 넣는 생성자 전용 함수다.
	InitBoxExtent(FVector(1000.f, 1000.f, 1000.f));
	ShapeColor = FColor(80, 200, 120);

	SetIsReplicatedByDefault(true);
}

void UCSObjectResetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCSObjectResetComponent, ResetCount);
	DOREPLIFETIME(UCSObjectResetComponent, LastResetReason);
}

void UCSObjectResetComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	HomeTransform = Owner->GetActorTransform();

	// 볼륨은 여기서 한 번만 읽는다. 이 컴포넌트는 오브젝트에 붙어 같이 움직이므로
	// 실시간 트랜스폼으로 판정하면 "범위를 벗어났다"가 영원히 성립하지 않는다.
	const FTransform BoundsWorld = GetComponentTransform();
	BoundsOrigin = BoundsWorld.GetLocation();
	BoundsRotation = BoundsWorld.GetRotation();
	BoundsExtent = GetScaledBoxExtent();

	if (UCSObjectResetSubsystem* Subsystem = UCSObjectResetSubsystem::Get(this))
	{
		Subsystem->Register(this);
	}

	// 위치 보정은 권한 쪽에서 해야 복제로 전파된다.
	if (Owner->HasAuthority() && bResetWhenOutOfBounds)
	{
		Owner->GetWorldTimerManager().SetTimer(
			BoundsCheckTimer, this, &UCSObjectResetComponent::CheckBounds,
			FMath::Max(BoundsCheckInterval, 0.02f), true);
	}
}

void UCSObjectResetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const AActor* Owner = GetOwner())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetTimerManager().ClearTimer(BoundsCheckTimer);
		}
	}

	if (UCSObjectResetSubsystem* Subsystem = UCSObjectResetSubsystem::Get(this))
	{
		Subsystem->Unregister(this);
	}

	Super::EndPlay(EndPlayReason);
}

bool UCSObjectResetComponent::IsOwnerInsideBounds() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || BoundsExtent.IsNearlyZero())
	{
		return true;
	}

	// BoundsExtent 는 이미 스케일이 반영된 값이라 회전만 되돌려서 비교한다.
	const FVector Local = BoundsRotation.UnrotateVector(Owner->GetActorLocation() - BoundsOrigin);

	return FMath::Abs(Local.X) <= BoundsExtent.X
		&& FMath::Abs(Local.Y) <= BoundsExtent.Y
		&& FMath::Abs(Local.Z) <= BoundsExtent.Z;
}

void UCSObjectResetComponent::CheckBounds()
{
	if (!bResetWhenOutOfBounds || IsOwnerInsideBounds())
	{
		return;
	}

	ResetToHome(ECSObjectResetReason::OutOfBounds);
}

void UCSObjectResetComponent::SetHomeToCurrentTransform()
{
	if (const AActor* Owner = GetOwner())
	{
		HomeTransform = Owner->GetActorTransform();
	}
}

bool UCSObjectResetComponent::ResetToHome(ECSObjectResetReason Reason)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	FTransform Target = HomeTransform;
	Target.SetLocation(HomeTransform.GetLocation() + ResetOffset);
	if (!bRestoreRotation)
	{
		Target.SetRotation(Owner->GetActorQuat());
	}

	Owner->SetActorTransform(Target, false, nullptr, ETeleportType::TeleportPhysics);

	// 되돌린 자리에서 떨어지던 속도 그대로 다시 날아가지 않게 정리한다.
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		if (RootPrimitive->IsSimulatingPhysics())
		{
			RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
			RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	Owner->ForceNetUpdate();
	Owner->FlushNetDormancy();

	LastResetReason = Reason;
	++ResetCount;

	// OnRep 은 서버 자신에게 오지 않으므로 여기서 직접 알린다.
	OnObjectReset.Broadcast(Reason);

	UE_LOG(LogCS, Log, TEXT("[ObjectReset] %s -> home (reason=%d)"),
		*Owner->GetName(), static_cast<int32>(Reason));

	return true;
}

void UCSObjectResetComponent::OnRep_ResetCount()
{
	OnObjectReset.Broadcast(LastResetReason);
}
