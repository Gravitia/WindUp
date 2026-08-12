// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSSplineRider.h"
#include "Actor/CSSplineTrack.h"
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interface/CSAbilitySource.h"
#include "Physics/CSCollision.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "ChronoSpace.h"

ACSSplineRider::ACSSplineRider()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// 능력 소스(블랙홀)의 물리 당김 대상으로 잡히지 않도록 오버랩 이벤트는 감지 스피어만 낸다.
	Mesh->SetGenerateOverlapEvents(false);

	AbilityDetection = CreateDefaultSubobject<USphereComponent>(TEXT("AbilityDetection"));
	AbilityDetection->SetupAttachment(Mesh);
	AbilityDetection->SetSphereRadius(DetectionRadius, true);
	AbilityDetection->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	AbilityDetection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	AbilityDetection->OnComponentBeginOverlap.AddDynamic(this, &ACSSplineRider::OnDetectionBeginOverlap);
	AbilityDetection->OnComponentEndOverlap.AddDynamic(this, &ACSSplineRider::OnDetectionEndOverlap);
}

void ACSSplineRider::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSSplineRider, RepDistance);
	DOREPLIFETIME(ACSSplineRider, bRailLocked);
}

void ACSSplineRider::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 트랙/시작 포인트를 지정하면 미리 그 위치로 스냅해 배치를 확인할 수 있게 한다.
	if (TargetTrack && TargetTrack->GetSpline())
	{
		ApplyLocationAtDistance(TargetTrack->GetDistanceAtPoint(StartPointIndex));
	}
}

void ACSSplineRider::BeginPlay()
{
	Super::BeginPlay();

	AbilityDetection->SetSphereRadius(DetectionRadius, true);

	if (!TargetTrack || !TargetTrack->GetSpline())
	{
		UE_LOG(LogCS, Warning, TEXT("[SplineRider] %s : TargetTrack 이 지정되지 않음. 이동하지 않는다."), *GetName());
		return;
	}

	StartDistance = TargetTrack->GetDistanceAtPoint(StartPointIndex);

	if (HasAuthority())
	{
		TargetTrack->RegisterRider(this);

		RepDistance = StartDistance;

		// 시작 위치가 트리거 포인트 위라면 통지 없이 도킹 상태로만 두어
		// 레벨 시작과 동시에 기믹이 발동하는 것을 막는다. (떠났다 돌아오면 발동)
		const TArray<FCSSplineTriggerPoint>& Entries = TargetTrack->GetTriggerPoints();
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (FMath::Abs(StartDistance - TargetTrack->GetDistanceAtPoint(Entries[i].PointIndex)) <= ArriveTolerance)
			{
				DockedEntryIndex = i;
				break;
			}
		}
	}

	TargetDistance = RepDistance;
	SmoothedDistance = RepDistance;
	ApplyLocationAtDistance(RepDistance);
}

void ACSSplineRider::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LockTimerHandle);

	if (HasAuthority() && TargetTrack)
	{
		TargetTrack->UnregisterRider(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ACSSplineRider::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!TargetTrack || !TargetTrack->GetSpline()) return;

	if (HasAuthority())
	{
		ServerMove(DeltaSeconds);
		TargetDistance = RepDistance;
	}
	else
	{
		SmoothedDistance = FMath::FInterpTo(SmoothedDistance, TargetDistance, DeltaSeconds, InterpSpeed);
		ApplyLocationAtDistance(SmoothedDistance);
	}
}

void ACSSplineRider::ServerMove(float DeltaSeconds)
{
	// 고정 중에는 능력 면역 + 위치 유지.
	if (bRailLocked) return;

	for (auto It = ActiveSources.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	USplineComponent* Spline = TargetTrack->GetSpline();
	const float SplineLength = Spline->GetSplineLength();
	const FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(RepDistance, ESplineCoordinateSpace::World).GetSafeNormal();

	// 라이더에 작용하는 힘은 "능력 + 경사(각도)" 둘뿐이다.
	// 경사 중력: 접선의 상하 성분만큼 내리막 방향으로 가속.
	const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.0f;
	float Accel = Tangent.Z * GravityZ * RailGravityScale;

	// 능력 소스들의 당김을 스플라인 접선에 투영해 1D 가속으로 변환.
	const FVector MyLocation = GetActorLocation();
	for (const TWeakObjectPtr<AActor>& Source : ActiveSources)
	{
		const FVector ToSource = Source->GetActorLocation() - MyLocation;
		Accel += FVector::DotProduct(ToSource.GetSafeNormal(), Tangent) * PullAcceleration;
	}

	float DesiredDistance;
	if (ActiveSources.Num() == 0 && MoveType == ECSSplineRiderMoveType::Recover && !FMath::IsNearlyEqual(RepDistance, StartDistance, 1.0f))
	{
		// Recover 타입은 능력이 안 닿는 동안 시작점 복귀가 경사보다 우선.
		RailVelocity = 0.0f;
		DesiredDistance = FMath::FInterpConstantTo(RepDistance, StartDistance, DeltaSeconds, RecoverSpeed);
	}
	else
	{
		RailVelocity += Accel * DeltaSeconds;
		RailVelocity *= FMath::Clamp(1.0f - MoveDamping * DeltaSeconds, 0.0f, 1.0f);
		RailVelocity = FMath::Clamp(RailVelocity, -MaxMoveSpeed, MaxMoveSpeed);

		// 평지에서 힘도 속도도 거의 없으면 정지 유지.
		if (FMath::IsNearlyZero(RailVelocity, 0.5f) && FMath::IsNearlyZero(Accel, 1.0f))
		{
			RailVelocity = 0.0f;
			return;
		}
		DesiredDistance = RepDistance + RailVelocity * DeltaSeconds;
	}

	DesiredDistance = FMath::Clamp(DesiredDistance, 0.0f, SplineLength);
	if (DesiredDistance <= 0.0f || DesiredDistance >= SplineLength)
	{
		RailVelocity = 0.0f;
	}

	// 같은 레일 라이더와의 점유/밀어내기 판정.
	const float FinalDistance = TargetTrack->ResolveRiderMove(this, DesiredDistance);
	if (!FMath::IsNearlyEqual(FinalDistance, DesiredDistance, 0.1f))
	{
		RailVelocity = 0.0f;
	}

	if (!FMath::IsNearlyEqual(FinalDistance, RepDistance, KINDA_SMALL_NUMBER))
	{
		SetServerDistance(FinalDistance);
	}
}

void ACSSplineRider::SetServerDistance(float NewDistance)
{
	RepDistance = NewDistance;   // 서버에서 값 변경 → 클라로 리플리케이트
	ApplyLocationAtDistance(NewDistance);

	// 도킹 중이면 이탈 판정만 한다. (떠나기 전까지 같은 포인트 재도달 판정 없음)
	if (DockedEntryIndex != INDEX_NONE)
	{
		const TArray<FCSSplineTriggerPoint>& Entries = TargetTrack->GetTriggerPoints();
		if (!Entries.IsValidIndex(DockedEntryIndex))
		{
			DockedEntryIndex = INDEX_NONE;
			return;
		}

		const float PointDistance = TargetTrack->GetDistanceAtPoint(Entries[DockedEntryIndex].PointIndex);
		if (FMath::Abs(NewDistance - PointDistance) > ArriveTolerance)
		{
			if (bRiderDebug)
			{
				UE_LOG(LogCS, Log, TEXT("[SplineRider] %s : TriggerPoint %d 이탈"), *GetName(), DockedEntryIndex);
			}
			TargetTrack->NotifyRiderLeftPoint(DockedEntryIndex);
			DockedEntryIndex = INDEX_NONE;
		}
		return;
	}

	// 새 트리거 포인트 도달 판정.
	const TArray<FCSSplineTriggerPoint>& Entries = TargetTrack->GetTriggerPoints();
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const float PointDistance = TargetTrack->GetDistanceAtPoint(Entries[i].PointIndex);
		if (FMath::Abs(NewDistance - PointDistance) <= ArriveTolerance)
		{
			DockAtTriggerPoint(i);
			break;
		}
	}
}

void ACSSplineRider::DockAtTriggerPoint(int32 TriggerEntryIndex)
{
	const FCSSplineTriggerPoint& Entry = TargetTrack->GetTriggerPoints()[TriggerEntryIndex];

	DockedEntryIndex = TriggerEntryIndex;
	RailVelocity = 0.0f;

	// 포인트 위치에 정확히 스냅.
	RepDistance = TargetTrack->GetDistanceAtPoint(Entry.PointIndex);
	ApplyLocationAtDistance(RepDistance);

	if (bRiderDebug)
	{
		UE_LOG(LogCS, Log, TEXT("[SplineRider] %s : TriggerPoint %d (SplinePoint %d) 도달, Lock %.1fs"),
			*GetName(), TriggerEntryIndex, Entry.PointIndex, Entry.LockDuration);
	}

	TargetTrack->NotifyRiderReachedPoint(TriggerEntryIndex);

	if (Entry.LockDuration > 0.0f)
	{
		SetRailLocked(true);
		GetWorldTimerManager().SetTimer(LockTimerHandle, this, &ACSSplineRider::EndRailLock, Entry.LockDuration, false);
	}
}

void ACSSplineRider::EndRailLock()
{
	SetRailLocked(false);
}

void ACSSplineRider::SetRailLocked(bool bNewLocked)
{
	if (bRailLocked == bNewLocked) return;

	bRailLocked = bNewLocked;   // 서버에서 값 변경 → 클라로 리플리케이트

	// RepNotify 는 서버 자신에겐 호출되지 않으므로 서버에서 직접 실행한다.
	HandleRailLockChanged();
}

void ACSSplineRider::ApplyPushedDistance(float NewDistance, float InheritVelocity)
{
	if (bRailLocked) return;

	RailVelocity = InheritVelocity;
	SetServerDistance(NewDistance);
}

void ACSSplineRider::OnDetectionBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepHitResult*/)
{
	if (!HasAuthority()) return;
	if (!IsAbilitySourceOfInterest(OtherActor)) return;

	ActiveSources.Add(OtherActor);

	if (bRiderDebug)
	{
		UE_LOG(LogCS, Log, TEXT("[SplineRider] %s : 능력 소스 진입 %s (총 %d)"), *GetName(), *GetNameSafe(OtherActor), ActiveSources.Num());
	}
}

void ACSSplineRider::OnDetectionEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority()) return;
	if (!IsAbilitySourceOfInterest(OtherActor)) return;

	ActiveSources.Remove(OtherActor);

	if (bRiderDebug)
	{
		UE_LOG(LogCS, Log, TEXT("[SplineRider] %s : 능력 소스 이탈 %s (총 %d)"), *GetName(), *GetNameSafe(OtherActor), ActiveSources.Num());
	}
}

bool ACSSplineRider::IsAbilitySourceOfInterest(const AActor* OtherActor) const
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

void ACSSplineRider::OnRep_RepDistance()
{
	TargetDistance = RepDistance;
}

void ACSSplineRider::OnRep_RailLocked()
{
	HandleRailLockChanged();
}

void ACSSplineRider::HandleRailLockChanged()
{
	if (bRailLocked)
	{
		OnRailLockStarted();
	}
	else
	{
		OnRailLockEnded();
	}
}

void ACSSplineRider::ApplyLocationAtDistance(float Distance)
{
	USplineComponent* Spline = TargetTrack ? TargetTrack->GetSpline() : nullptr;
	if (!Spline) return;

	SetActorLocation(Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
}
