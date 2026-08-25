// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSClawMachine.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACSClawMachine::ACSClawMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Claw = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Claw"));
	SetRootComponent(Claw);
	Claw->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 기본 외형 — 엔진 기본 큐브. 디자이너가 BP에서 교체 가능
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		Claw->SetStaticMesh(CubeMeshAsset.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Claw);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
}

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */

void ACSClawMachine::BeginPlay()
{
	Super::BeginPlay();

	// 초기 transform 캐시 — Home / Destination 계산 기준점
	HomeTransform = GetActorTransform();

	// 트리거는 Claw의 자식이지만 World 위치를 독립적으로 유지해야 한다
	// (클로가 내려가도 트리거는 지면 근처 자기 자리에 그대로 남아야 함)
	Trigger->SetUsingAbsoluteLocation(true);
	Trigger->SetUsingAbsoluteRotation(true);
	Trigger->SetUsingAbsoluteScale(true);

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACSClawMachine::OnTriggerBegin);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &ACSClawMachine::OnTriggerEnd);
	}
}

void ACSClawMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSClawMachine, State);
	DOREPLIFETIME(ACSClawMachine, GrabbedPlayer);
}

/* ─────────────────────────────────────────────
 *  Tick — 서버 전용 상태 진행
 * ───────────────────────────────────────────── */

void ACSClawMachine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) return;

	switch (State)
	{
	case EClawMachineState::Home:
		break;

	case EClawMachineState::Diving:
		if (MoveClawTowards(TargetLocation, DiveSpeed, DeltaTime))
		{
			GrabPlayer();
		}
		break;

	case EClawMachineState::Retracting:
		if (MoveClawTowards(HomeTransform.GetLocation(), RetractSpeed, DeltaTime))
		{
			State = EClawMachineState::Home;
			TryGrabRemainingPlayer();
		}
		break;

	case EClawMachineState::Lifting:
		if (MoveClawTowards(TargetLocation, LiftSpeed, DeltaTime))
		{
			// 좌우 이동 시작 — 현재 z를 유지한 채 목적지 xy로
			const FVector Current = GetActorLocation();
			const FVector DestWorld = GetDestinationWorld();
			TargetLocation = FVector(DestWorld.X, DestWorld.Y, Current.Z);
			State = EClawMachineState::Translating;
		}
		break;

	case EClawMachineState::Translating:
		if (MoveClawTowards(TargetLocation, TranslateSpeed, DeltaTime))
		{
			// 내림 시작 — 목적지 z까지
			TargetLocation = GetDestinationWorld();
			State = EClawMachineState::Dropping;
		}
		break;

	case EClawMachineState::Dropping:
		if (MoveClawTowards(TargetLocation, DropSpeed, DeltaTime))
		{
			ReleasePlayer();
			State = EClawMachineState::Returning;
		}
		break;

	case EClawMachineState::Returning:
		if (MoveClawTowards(HomeTransform.GetLocation(), ReturnSpeed, DeltaTime))
		{
			State = EClawMachineState::Home;
			TryGrabRemainingPlayer();
		}
		break;
	}
}

/* ─────────────────────────────────────────────
 *  Trigger callbacks (서버 전용)
 * ───────────────────────────────────────────── */

FVector ACSClawMachine::GetDestinationWorld() const
{
	// MakeEditWidget 프로퍼티는 액터 로컬 공간으로 저장된다 - 뷰포트에서 찍은 자리로 가려면 변환이 필요하다.
	return HomeTransform.TransformPosition(Destination);
}

void ACSClawMachine::TryGrabRemainingPlayer()
{
	if (!HasAuthority() || State != EClawMachineState::Home) return;

	// 여러 명이 트리거 안에 있을 때 한 명을 옮기고 돌아오면 남은 사람의 BeginOverlap 은 다시 오지 않는다.
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, ACSCharacterPlayer::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(Actor))
		{
			StartDive(Player);
			return;
		}
	}
}

void ACSClawMachine::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (State != EClawMachineState::Home) return;

	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (!Player) return;

	StartDive(Player);
}

void ACSClawMachine::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 다이빙 중 잡으려던 플레이어가 빠져나간 경우만 복귀
	if (State != EClawMachineState::Diving) return;
	if (OtherActor != GrabbedPlayer) return;

	StartRetract();
}

/* ─────────────────────────────────────────────
 *  State transitions
 * ───────────────────────────────────────────── */

void ACSClawMachine::StartDive(ACSCharacterPlayer* Target)
{
	GrabbedPlayer = Target;
	State = EClawMachineState::Diving;

	// 클로는 현재 xy를 유지한 채 트리거 z까지 수직 강하
	const FVector Current    = GetActorLocation();
	const FVector TriggerLoc = Trigger->GetComponentLocation();
	TargetLocation = FVector(Current.X, Current.Y, TriggerLoc.Z);

	ForceNetUpdate();
}

void ACSClawMachine::StartRetract()
{
	GrabbedPlayer = nullptr;
	State = EClawMachineState::Retracting;

	ForceNetUpdate();
}

void ACSClawMachine::GrabPlayer()
{
	// 도착 시점에 잡으려던 플레이어가 사라졌으면 (방어적 처리)
	if (!IsValid(GrabbedPlayer))
	{
		StartRetract();
		return;
	}

	GrabbedPlayer->AttachToComponent(Claw, FAttachmentTransformRules::KeepWorldTransform);
	if (UCharacterMovementComponent* Movement = GrabbedPlayer->GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// 들어올리기 시작 (현재 위치에서 LiftHeight만큼 상승)
	const FVector Current = GetActorLocation();
	TargetLocation = Current + FVector(0.0f, 0.0f, LiftHeight);
	State = EClawMachineState::Lifting;

	ForceNetUpdate();
}

void ACSClawMachine::ReleasePlayer()
{
	if (!IsValid(GrabbedPlayer)) { GrabbedPlayer = nullptr; return; }

	GrabbedPlayer->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCharacterMovementComponent* Movement = GrabbedPlayer->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	GrabbedPlayer = nullptr;

	ForceNetUpdate();
}

/* ─────────────────────────────────────────────
 *  Helpers
 * ───────────────────────────────────────────── */

bool ACSClawMachine::MoveClawTowards(const FVector& Goal, float Speed, float DeltaTime)
{
	const FVector Current  = GetActorLocation();
	const FVector Delta    = Goal - Current;
	const float   Distance = Delta.Size();
	const float   Step     = Speed * DeltaTime;

	if (Distance <= Step || Distance <= KINDA_SMALL_NUMBER)
	{
		SetActorLocation(Goal);
		return true;
	}

	SetActorLocation(Current + Delta / Distance * Step);
	return false;
}
