// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSPartyPopper.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "ChronoSpace.h"

ACSPartyPopper::ACSPartyPopper()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true; // 에디터 원뿔 미리보기 위해 초기 활성
	bReplicates = true;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetIsReplicated(true);
	RootComponent = BodyMesh;

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(BodyMesh);

	LaunchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchPoint"));
	LaunchPoint->SetupAttachment(BodyMesh);

	EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(BodyMesh);

	StringMesh = CreateDefaultSubobject<USplineMeshComponent>(TEXT("StringMesh"));
	StringMesh->SetupAttachment(RootComponent);
	StringMesh->SetMobility(EComponentMobility::Movable);
	StringMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACSPartyPopper::BeginPlay()
{
	Super::BeginPlay();

	// 뚜껑 초기 트랜스폼 캐시 (bOneShot=false 시 리셋 용도)
	if (IsValid(LidMesh))
	{
		InitialLidRelTransform = LidMesh->GetRelativeTransform();
	}

	// 게임에서 거리 체크 대상이 없으면 Tick 비활성 (폭발 시 다시 켜짐)
	if (!IsValid(LinkedButtonActor))
	{
		SetActorTickEnabled(false);
	}
}

/* ════════════════════════════════════
 *  ICSInteractionInterface
 * ════════════════════════════════════ */

void ACSPartyPopper::BeginInteraction()
{
}

void ACSPartyPopper::EndInteraction()
{
}

void ACSPartyPopper::Interact()
{
	Explode();
}

void ACSPartyPopper::Explode()
{
	if (!HasAuthority()) return;
	if (bOneShot && bHasFired) return;

	bHasFired = true;

	TArray<FVector> Velocities;
	TArray<int32>   MeshIndices;
	Velocities.Reserve(ParticleCount);
	MeshIndices.Reserve(ParticleCount);

	const int32 NumTypes = FMath::Max(1, ParticleMeshTypes.Num());

	const FVector WorldDir = GetActorTransform()
		.TransformVectorNoScale(LaunchDirection)
		.GetSafeNormal();
	const float HalfAngleRad = FMath::DegreesToRadians(ConeHalfAngle);

	for (int32 i = 0; i < ParticleCount; ++i)
	{
		const FVector Dir  = FMath::VRandCone(WorldDir, HalfAngleRad);
		const float   Speed = ExplosionSpeed * FMath::RandRange(SpeedRandomMin, SpeedRandomMax);
		Velocities.Add(Dir * Speed);
		MeshIndices.Add(FMath::RandRange(0, NumTypes - 1));
	}

	Multicast_Explode(Velocities, MeshIndices);
}

/* ════════════════════════════════════
 *  Tick
 * ════════════════════════════════════ */

void ACSPartyPopper::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── 연결 줄 업데이트 (에디터·게임 모두) ──
	UpdateStringMesh();

#if ENABLE_DRAW_DEBUG
	// ── 에디터 전용: 원뿔 미리보기 ──
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		const FVector Origin   = IsValid(LaunchPoint) ? LaunchPoint->GetComponentLocation() : GetActorLocation();
		const FVector WorldDir = GetActorTransform().TransformVectorNoScale(LaunchDirection).GetSafeNormal();
		const float   HalfRad  = FMath::DegreesToRadians(ConeHalfAngle);

		DrawDebugCone(GetWorld(), Origin, WorldDir, 150.0f, HalfRad, HalfRad,
		              16, FColor::Yellow, false, -1.0f, 0, 0.5f);
		return;
	}
#endif

	// ── 거리 트리거 체크 (서버만) ──
	if (HasAuthority() && IsValid(LinkedButtonActor) && !(bOneShot && bHasFired))
	{
		const float Dist = FVector::Dist(GetActorLocation(), LinkedButtonActor->GetActorLocation());
		if (Dist >= TriggerDistance)
		{
			Explode();
		}
	}

	TickLid(DeltaTime);
	TickParticles(DeltaTime);
	TickRecoil(DeltaTime);

	// 뚜껑 + 입자 모두 끝나면 정리 (발사 타이머 대기 중이면 정리 안 함)
	if (bIsExploding && !bLaunchPending && !LidState.bFlying && !bSimulating)
	{
		CleanupParticles();
	}
}

void ACSPartyPopper::TickLid(float DeltaTime)
{
	if (!LidState.bFlying || !IsValid(LidMesh)) return;

	LidState.Elapsed += DeltaTime;

	if (LidState.Elapsed >= ParticleLifeTime)
	{
		LidState.bFlying = false;
		LidMesh->SetVisibility(false);
		return;
	}

	LidState.Velocity.Z -= GravityZ * DeltaTime;
	LidMesh->SetRelativeLocation(LidMesh->GetRelativeLocation() + LidState.Velocity * DeltaTime);
	LidMesh->AddRelativeRotation(LidState.AngularVelocity * DeltaTime);
}

void ACSPartyPopper::TickRecoil(float DeltaTime)
{
	if (!RecoilState.bActive) return;

	// 빠른 감속: 매 프레임 속도를 급격히 줄여 0.2~0.3초 안에 멈춤
	RecoilState.Velocity *= FMath::Max(0.0f, 1.0f - DeltaTime * 12.0f);

	if (RecoilState.Velocity.SizeSquared() < 1.0f)
	{
		RecoilState.bActive = false;
		return;
	}

	AddActorLocalOffset(RecoilState.Velocity * DeltaTime, true);
}

void ACSPartyPopper::TickParticles(float DeltaTime)
{
	if (!bSimulating) return;

	bool bAnyAlive = false;

	for (int32 i = 0; i < ParticleComps.Num(); ++i)
	{
		FParticleState& State = ParticleStates[i];

		if (State.Elapsed >= ParticleLifeTime) continue;

		State.Elapsed += DeltaTime;

		if (State.Elapsed >= ParticleLifeTime)
		{
			ParticleComps[i]->SetVisibility(false);
			continue;
		}

		bAnyAlive = true;

		State.Velocity.Z -= GravityZ * DeltaTime;

		const FVector NewLoc = ParticleComps[i]->GetRelativeLocation() + State.Velocity * DeltaTime;
		ParticleComps[i]->SetRelativeLocation(NewLoc);
		ParticleComps[i]->AddRelativeRotation(State.AngularVelocity * DeltaTime);

		const float LifeRatio = 1.0f - (State.Elapsed / ParticleLifeTime);
		ParticleComps[i]->SetRelativeScale3D(FVector(ParticleScale * LifeRatio));
	}

	if (!bAnyAlive)
	{
		bSimulating = false;
	}
}

/* ════════════════════════════════════
 *  네트워크
 * ════════════════════════════════════ */

void ACSPartyPopper::Multicast_Explode_Implementation(const TArray<FVector>& Velocities, const TArray<int32>& MeshIndices)
{
	DoExplode(Velocities, MeshIndices);
}

/* ════════════════════════════════════
 *  내부 함수
 * ════════════════════════════════════ */

void ACSPartyPopper::DoExplode(const TArray<FVector>& Velocities, const TArray<int32>& MeshIndices)
{
	CleanupParticles();

	// ── 본체 반동 즉시 시작 ──
	RecoilState.Velocity = RecoilDirection.GetSafeNormal() * RecoilSpeed;
	RecoilState.bActive  = true;

	bIsExploding = true;
	SetActorTickEnabled(true);

	// ── 컨페티·뚜껑·사운드는 LaunchDelay 후 발사 ──
	PendingVelocities   = Velocities;
	PendingMeshIndices  = MeshIndices;

	bLaunchPending = true;

	if (LaunchDelay <= 0.0f)
	{
		DoLaunch();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			LaunchTimerHandle,
			this, &ACSPartyPopper::DoLaunch,
			LaunchDelay, false
		);
	}
}

void ACSPartyPopper::DoLaunch()
{
	// ── 사운드 재생 ──
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	}

	// ── 뚜껑 날리기 ──
	if (IsValid(LidMesh))
	{
		const FVector WorldDir = GetActorTransform()
			.TransformVectorNoScale(LaunchDirection)
			.GetSafeNormal();

		LidState.Velocity = WorldDir * LidLaunchSpeed + FMath::VRand() * (LidLaunchSpeed * 0.2f);
		LidState.AngularVelocity = FRotator(
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed),
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed),
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed)
		);
		LidState.Elapsed = 0.0f;
		LidState.bFlying = true;
	}

	// ── 파티클 생성 ──
	const int32   Count        = PendingVelocities.Num();
	const FVector SpawnWorldLoc = IsValid(LaunchPoint)
		? LaunchPoint->GetComponentLocation()
		: GetActorLocation();

	ParticleComps.Reserve(Count);
	ParticleStates.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetWorldLocation(SpawnWorldLoc);
		Comp->SetRelativeScale3D(FVector(ParticleScale));
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetIsReplicated(false);
		Comp->RegisterComponent();

		if (ParticleMeshTypes.IsValidIndex(PendingMeshIndices[i]) && ParticleMeshTypes[PendingMeshIndices[i]])
		{
			Comp->SetStaticMesh(ParticleMeshTypes[PendingMeshIndices[i]]);
		}

		ParticleComps.Add(Comp);

		FParticleState State;
		State.Velocity = PendingVelocities[i];
		State.Elapsed  = 0.0f;
		State.AngularVelocity = FRotator(
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed),
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed),
			FMath::RandRange(-MaxRotationSpeed, MaxRotationSpeed)
		);
		ParticleStates.Add(State);
	}

	bSimulating    = true;
	bLaunchPending = false;

	PendingVelocities.Empty();
	PendingMeshIndices.Empty();
}

void ACSPartyPopper::UpdateStringMesh()
{
	if (!IsValid(StringMesh)) return;

	if (!IsValid(LinkedButtonActor))
	{
		StringMesh->SetVisibility(false);
		return;
	}

	StringMesh->SetVisibility(true);

	const FTransform& MyInv = GetActorTransform().Inverse();

	// 시작점: EndPoint 위치 (없으면 액터 원점)
	const FVector LocalStart = IsValid(EndPoint)
		? MyInv.TransformPosition(EndPoint->GetComponentLocation())
		: FVector::ZeroVector;

	// 끝점: 버튼 로컬 오프셋 적용 후 이 액터 로컬 스페이스로 변환
	const FVector ButtonWorldLoc = LinkedButtonActor->GetActorTransform().TransformPosition(StringEndOffset);
	const FVector LocalEnd       = MyInv.TransformPosition(ButtonWorldLoc);

	// 중간점을 아래로 내려 처짐 표현 (quadratic → cubic Hermite 근사)
	const FVector Midpoint     = (LocalStart + LocalEnd) * 0.5f - FVector(0.0f, 0.0f, StringSag);
	const FVector StartTangent = (Midpoint - LocalStart) * 2.0f;
	const FVector EndTangent   = (LocalEnd  - Midpoint)  * 2.0f;

	StringMesh->SetStartAndEnd(LocalStart, StartTangent, LocalEnd, EndTangent, true);
}

void ACSPartyPopper::CleanupParticles()
{
	GetWorldTimerManager().ClearTimer(LaunchTimerHandle);
	PendingVelocities.Empty();
	PendingMeshIndices.Empty();
	bLaunchPending = false;

	for (UStaticMeshComponent* Comp : ParticleComps)
	{
		if (IsValid(Comp))
		{
			Comp->DestroyComponent();
		}
	}
	ParticleComps.Empty();
	ParticleStates.Empty();
	bIsExploding = false;

	// bOneShot=false 이면 뚜껑 복원 + 재발사 가능 상태로 리셋
	if (!bOneShot)
	{
		bHasFired    = false;
		LidState     = FLidState{};
		if (IsValid(LidMesh))
		{
			LidMesh->SetRelativeTransform(InitialLidRelTransform);
			LidMesh->SetVisibility(true);
		}
	}

	// 거리 체크가 더 필요하면 Tick 유지
	const bool bNeedDistanceCheck = IsValid(LinkedButtonActor) && !(bOneShot && bHasFired);
	if (!bNeedDistanceCheck)
	{
		SetActorTickEnabled(false);
	}
}
