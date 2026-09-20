// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/CSTrampoline.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "ChronoSpace.h"

/**
 * 1 이면 튕기는 방향(파란 화살표)과 방금 발사한 속도(초록 화살표)를 잠깐 그린다.
 * 기획자가 세기와 각도를 맞출 때 쓰라고 둔 것이다. cs.Conveyor.DebugDraw 와 같은 용도.
 */
static TAutoConsoleVariable<int32> CVarCSTrampolineDebugDraw(
	TEXT("cs.Trampoline.DebugDraw"),
	0,
	TEXT("0=off, 1=튕기는 방향과 발사 속도를 그린다"),
	ECVF_Cheat);

ACSTrampoline::ACSTrampoline()
{
	// 트리거 안에 캐릭터가 있을 때만 켠다. 빈 트램펄린은 틱하지 않는다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 발사는 서버와 조종 클라에서 각각 계산한다. 복제할 상태가 없다.
	// 다만 레벨 배치 액터로서 클라에도 존재해야 하므로 기본값을 그대로 둔다.
	bReplicates = false;

	// 루트를 빈 씬 컴포넌트로 둔다. 그래야 메시와 트리거를 BP 에서 자유롭게 옮길 수 있다.
	// 메시를 루트로 잡으면 메시의 상대 위치가 곧 액터 위치라, 피벗이 한쪽으로 치우친
	// 에셋을 끼웠을 때 보정할 방법이 없다.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	BounceTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("BounceTrigger"));
	BounceTrigger->SetupAttachment(RootComponent);
	BounceTrigger->SetBoxExtent(FVector(100.f, 100.f, 40.f));
	// 메시 위에 얹어 둔다. 밟는 순간 겹치도록.
	BounceTrigger->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	BounceTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void ACSTrampoline::BeginPlay()
{
	Super::BeginPlay();

	BounceTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSTrampoline::OnBounceBeginOverlap);
	BounceTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSTrampoline::OnBounceEndOverlap);
}

void ACSTrampoline::OnBounceBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!CanLaunch(Character))
	{
		return;
	}

	// 판정은 여기서. 발사는 다음 틱에서. 이유는 PendingLaunch 선언부 주석 참고.
	// 부딪히던 그 순간의 속도를 같이 저장한다. 다음 틱에는 이미 착지해 0 이 되어 있어서
	// 수평 관성과 "더하기" 모드에 쓸 값을 잃는다.
	PendingLaunch.Add(Character, Character->GetVelocity());
	SetActorTickEnabled(true);
}

void ACSTrampoline::OnBounceEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// 나가는 것 자체로는 할 일이 없다. 예약이 남아 있으면 다음 틱에 처리된다.
	// (빠르게 통과해 같은 프레임에 들어왔다 나가도 발사는 보장된다)
}

void ACSTrampoline::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (const TPair<TWeakObjectPtr<ACharacter>, FVector>& Entry : PendingLaunch)
	{
		if (ACharacter* Character = Entry.Key.Get())
		{
			LaunchNow(Character, Entry.Value);
		}
	}

	PendingLaunch.Reset();
	SetActorTickEnabled(false);
}

FVector ACSTrampoline::GetLaunchDirection() const
{
	// 월드 Z 가 아니라 액터의 위쪽이다. 기울여 놓으면 비스듬히, 뒤집어 놓으면 아래로 쏜다.
	// 중력 반전으로 천장을 밟고 있는 상황에서도 배치한 대로 동작한다.
	return GetActorUpVector();
}

bool ACSTrampoline::CanLaunch(const ACharacter* Character) const
{
	if (!IsValid(Character) || !Character->GetCharacterMovement())
	{
		return false;
	}

	if (bPlayersOnly && !Character->IsPlayerControlled())
	{
		return false;
	}

	// 그 캐릭터를 실제로 시뮬레이션하는 머신에서만 발사한다.
	// 다른 클라이언트의 시뮬레이션 프록시까지 밀면 같은 발사가 두 번 들어간다.
	if (!Character->HasAuthority() && !Character->IsLocallyControlled())
	{
		return false;
	}

	if (RetriggerCooldown > 0.f)
	{
		const UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		if (const float* Last = LastLaunchTime.Find(Character))
		{
			if (World->GetTimeSeconds() - *Last < RetriggerCooldown)
			{
				return false;
			}
		}
	}

	// 충분히 빠르게 "내려와 부딪혔을" 때만 튕긴다.
	//
	// 이 조건 하나가 두 가지를 동시에 해결한다.
	//   - 걸어서 지나가거나 위에 가만히 서 있을 때는 튕기지 않는다. 트램펄린을 밟았다고
	//     제멋대로 날아가는 것은 원치 않는 동작이다.
	//   - 방금 튕겨 올라가는 캐릭터를 트리거 안에 있다는 이유로 매 프레임 또 튕겨
	//     속도가 무한히 쌓이는 것도 같이 막힌다. 올라가는 중에는 부호가 반대다.
	//
	// 속도는 월드 Z 가 아니라 트램펄린이 튕기는 방향 기준으로 잰다. 기울여 놓거나
	// 중력이 반전돼도 "정면으로 부딪혔는가" 로 판정된다.
	const float SpeedAlongUp = FVector::DotProduct(Character->GetVelocity(), GetLaunchDirection());
	if (SpeedAlongUp > -MinLandingSpeed)
	{
		return false;
	}

	return true;
}

void ACSTrampoline::LaunchNow(ACharacter* Character, const FVector& ImpactVelocity)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Character))
	{
		return;
	}

	const FVector Up = GetLaunchDirection();
	const FVector Velocity = ImpactVelocity;

	// 기존 속도를 "튕기는 방향" 과 "그에 수직인 방향" 으로 나눈다.
	// 월드 축(XY/Z)으로 나누면 중력이 반전됐거나 트램펄린이 기울어졌을 때 어긋난다.
	const FVector AlongUp = Up * FVector::DotProduct(Velocity, Up);
	const FVector Horizontal = Velocity - AlongUp;

	FVector LaunchVelocity = Horizontal * HorizontalVelocityScale + Up * LaunchSpeed;
	if (!bOverrideLaunchSpeed)
	{
		// 더하기 모드: 떨어지던 속도를 그대로 살려 더 높이 튀게 한다.
		// 아래로 떨어지던 성분은 부호가 반대라 빼 준다.
		LaunchVelocity += -AlongUp;
	}

	// 축 단위로 합치는 엔진 기본 동작을 쓰지 않으려고 둘 다 override 로 넘긴다.
	// 최종 속도는 위에서 이미 다 계산했다.
	//
	// 이 호출은 반드시 액터 Tick 에서 해야 한다. 오버랩 콜백 안에서 부르면 먹지 않는다.
	// UCharacterMovementComponent::PerformMovement 의 순서가
	//   HandlePendingLaunch() -> ClearAccumulatedForces() -> StartNewPhysics()
	// 인데, 오버랩은 맨 뒤 StartNewPhysics 안에서 발생하므로 그때 예약한 발사는
	// 이미 지나간 HandlePendingLaunch 를 놓치고 같은 프레임의 ClearAccumulatedForces 에
	// 지워진다. 엔진 소스에도 같은 함정이 주석으로 적혀 있다.
	Character->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/true);

	LastLaunchTime.Add(Character, World->GetTimeSeconds());
	for (auto It = LastLaunchTime.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	UE_LOG(LogCS, Verbose,
		TEXT("Trampoline %s: launch %s speed=%.0f (Authority=%d Local=%d)"),
		*GetName(), *Character->GetName(), LaunchVelocity.Size(),
		Character->HasAuthority() ? 1 : 0,
		Character->IsLocallyControlled() ? 1 : 0);

#if !UE_BUILD_SHIPPING
	if (CVarCSTrampolineDebugDraw.GetValueOnGameThread() != 0)
	{
		const FVector Start = GetActorLocation();
		DrawDebugDirectionalArrow(World, Start, Start + Up * 200.f, 40.f, FColor::Blue, false, 2.f, 0, 4.f);
		DrawDebugDirectionalArrow(World, Character->GetActorLocation(),
			Character->GetActorLocation() + LaunchVelocity * 0.2f, 40.f, FColor::Green, false, 2.f, 0, 4.f);
		DrawDebugString(World, Start + Up * 220.f,
			FString::Printf(TEXT("%s  launch %.0f"), *GetName(), LaunchVelocity.Size()),
			nullptr, FColor::Blue, 2.f, true);
	}
#endif

	OnLaunched(Character, LaunchVelocity);
}

