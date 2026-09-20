// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorManager.h"
#include "Actor/CSConveyorPlatform.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

/**
 * 1 이면 벨트 진행 방향(노란 화살표)과 지금 밀리고 있는 캐릭터(초록 선)를 그린다.
 * 기획자가 RiderPushSpeed 부호를 맞출 때 쓰라고 둔 것이다.
 */
static TAutoConsoleVariable<int32> CVarCSConveyorDebugDraw(
	TEXT("cs.Conveyor.DebugDraw"),
	0,
	TEXT("0=off, 1=벨트 진행 방향과 밀리는 캐릭터를 그린다"),
	ECVF_Cheat);

ACSConveyorManager::ACSConveyorManager()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	ConveyorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConveyorISM"));
	ConveyorISM->SetupAttachment(RootComponent);
	ConveyorISM->SetMobility(EComponentMobility::Movable);
	// 기존: NoCollision
	ConveyorISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 가장 쉬운 방법: 프로필 사용
	ConveyorISM->SetCollisionProfileName(TEXT("BlockAll"));

}

void ACSConveyorManager::BeginPlay()
{
	Super::BeginPlay();

	InitializePlatformsFromAssigned();
	// BuildVisual(); // 필요하면 호출

	// 첫 프레임부터 정답 위치에서 시작한다. 0 에서 출발해 따라가면 벨트가 한 번 튄다.
	TargetProgress = ComputeProgressFromServerTime();
	SmoothedProgress = TargetProgress;
}

float ACSConveyorManager::ComputeProgressFromServerTime() const
{
	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	// 클라에서도 동기화되어 있는 서버 시계를 쓴다. 접속 직후 GameState 가 아직
	// 도착하지 않았으면 로컬 시간으로 대신한다 (몇 프레임 뒤 알아서 맞춰진다).
	const AGameStateBase* GameState = World->GetGameState();
	const double ServerTime = GameState
		? (double)GameState->GetServerWorldTimeSeconds()
		: (double)World->GetTimeSeconds();

	// 기존과 같은 방향: 시간이 흐르면 진행값이 줄어든다.
	double Progress = FMath::Fmod(-(double)MoveSpeed * ServerTime, (double)TotalLength);
	if (Progress < 0.0)
	{
		Progress += (double)TotalLength;
	}

	return (float)Progress;
}

void ACSConveyorManager::InitializePlatformsFromAssigned()
{
	Platforms.Empty();

	// 1) null 제거하면서 순서 유지
	for (ACSConveyorPlatform* P : AssignedPlatforms)
	{
		if (!P)
		{
			continue;
		}

		Platforms.Add(P);
	}

	// 2) 인덱스 기반 오프셋 적용
	for (int32 i = 0; i < Platforms.Num(); ++i)
	{
		ACSConveyorPlatform* P = Platforms[i];
		if (!P)
		{
			continue;
		}

		P->SetManager(this);
		P->SetIndexOffset((float)i * ConveyorSpacing);
	}

	// 3) 총 길이는 "플랫폼 개수 * spacing"
	TotalLength = (float)Platforms.Num() * ConveyorSpacing;

	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		TotalLength = 0.f;
	}
}


void ACSConveyorManager::AlignAssignedPlatforms()
{
	int32 Index = 0;
	int32 SkippedDuplicates = 0;
	TSet<ACSConveyorPlatform*> Seen;

	for (ACSConveyorPlatform* Platform : AssignedPlatforms)
	{
		if (!IsValid(Platform))
		{
			continue;
		}

		// 같은 플랫폼을 두 번 넣으면 런타임에도 순번이 하나만 살아남아 벨트에 구멍이 생긴다.
		// 정렬 단계에서 미리 걸러 알려 준다.
		bool bAlreadySeen = false;
		Seen.Add(Platform, &bAlreadySeen);
		if (bAlreadySeen)
		{
			++SkippedDuplicates;
			continue;
		}

#if WITH_EDITOR
		Platform->Modify();
#endif

		const FVector LocalOffset(0.f, (float)Index * ConveyorSpacing, Platform->GetZOffset());
		const FTransform ManagerTM = GetActorTransform();

		Platform->SetActorLocationAndRotation(
			ManagerTM.TransformPosition(LocalOffset),
			ManagerTM.GetRotation()
		);

		++Index;
	}

	UE_LOG(LogTemp, Log,
		TEXT("%s: AlignAssignedPlatforms - %d개 정렬, 간격 %.1f, 총 길이 %.1f%s"),
		*GetName(), Index, ConveyorSpacing, (float)Index * ConveyorSpacing,
		SkippedDuplicates > 0
			? *FString::Printf(TEXT(" (중복 %d개 건너뜀 - AssignedPlatforms 를 확인할 것)"), SkippedDuplicates)
			: TEXT(""));
}

void ACSConveyorManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 라이더 밀어주기는 플랫폼이 한 장도 없는 벨트(ISM 만 쓰는 형태)에서도 동작해야 하므로
	// 아래 TotalLength 조기 반환보다 먼저 처리한다.
	UpdateRiderPush(DeltaSeconds);

	if (TotalLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 서버든 클라든 같은 식으로 계산한다. 복제하지 않는다.
	TargetProgress = ComputeProgressFromServerTime();

	if (HasAuthority())
	{
		// 서버에서는 계산값이 곧 정답이다. 여기에 보간을 걸면 이유 없이 뒤처진다.
		// (예전 코드는 서버도 보간을 타서 MoveSpeed/InterpSpeed 만큼 영구 지연이 있었다)
		SmoothedProgress = TargetProgress;
		return;
	}

	// ===== Client smoothing =====
	// 서버 시계 추정치가 0.1초마다 갱신될 때 생기는 미세한 튐만 흡수한다.
	const float Delta = FMath::Abs(TargetProgress - SmoothedProgress);

	if (Delta > TotalLength * 0.5f)
	{
		// 한 바퀴 돌아 0 으로 넘어간 순간. 보간하면 벨트가 거꾸로 훑고 지나간다.
		SmoothedProgress = TargetProgress;
	}
	else
	{
		SmoothedProgress = FMath::FInterpTo(
			SmoothedProgress,
			TargetProgress,
			DeltaSeconds,
			ClientSmoothingSpeed
		);
	}
}

FVector ACSConveyorManager::GetBeltDirection() const
{
	// 플랫폼은 로컬 오프셋 (0, D, ZOffset) 에 놓이고 D 는 진행값과 함께 줄어든다.
	// 따라서 MoveSpeed 가 양수면 로컬 -Y, 즉 월드 -Right 방향으로 흐른다.
	// MoveSpeed 가 0 이면 흐르지 않지만, 미는 방향의 기준은 필요하므로 같은 축을 쓴다.
	const float DirSign = (MoveSpeed < 0.f) ? -1.f : 1.f;
	return -GetActorRightVector() * DirSign;
}

bool ACSConveyorManager::IsMyPlatform(const AActor* InActor) const
{
	if (!InActor)
	{
		return false;
	}

	for (const TObjectPtr<ACSConveyorPlatform>& Platform : Platforms)
	{
		if (Platform == InActor)
		{
			return true;
		}
	}
	return false;
}

void ACSConveyorManager::UpdateRiderPush(float DeltaSeconds)
{
	// 쓰지 않는 벨트에서 매 프레임 액터를 훑지 않도록 먼저 빠진다.
	// 블렌드가 남아 있으면(방금 0으로 바꿨거나 캐릭터가 내리는 중) 마저 처리해야 한다.
	if (FMath::IsNearlyZero(RiderPushSpeed) && RiderBlend.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector BeltDir = GetBeltDirection();
	const bool bDebug = CVarCSConveyorDebugDraw.GetValueOnGameThread() != 0;

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		if (!IsValid(Character))
		{
			continue;
		}

		// 그 캐릭터를 실제로 시뮬레이션하는 머신에서만 민다.
		// 다른 클라이언트의 시뮬레이션 프록시는 서버 결과가 복제되어 따라오므로
		// 여기서 또 밀면 같은 이동이 두 번 들어가 캐릭터가 앞서 나간다.
		// (ACSConveyorPlatform::DetachRidersOnWrap 과 같은 기준이다)
		if (!Character->HasAuthority() && !Character->IsLocallyControlled())
		{
			continue;
		}

		const UPrimitiveComponent* Base = Character->GetMovementBase();
		const AActor* BaseOwner = Base ? Base->GetOwner() : nullptr;

		// 매니저 자신(ISM 벨트)을 밟고 있어도 올라탄 것으로 본다.
		const bool bRiding = (BaseOwner != nullptr) && (BaseOwner == this || IsMyPlatform(BaseOwner));

		float* ExistingAlpha = RiderBlend.Find(Character);
		if (!bRiding && ExistingAlpha == nullptr)
		{
			// 탄 적도 없고 블렌드도 없다. 맵에 항목을 만들지 않는다.
			continue;
		}

		float& Alpha = ExistingAlpha ? *ExistingAlpha : RiderBlend.Add(Character, 0.f);

		const float TargetAlpha = bRiding ? 1.f : 0.f;
		Alpha = (RiderBlendTime > KINDA_SMALL_NUMBER)
			? FMath::FInterpConstantTo(Alpha, TargetAlpha, DeltaSeconds, 1.f / RiderBlendTime)
			: TargetAlpha;

		if (Alpha > KINDA_SMALL_NUMBER)
		{
			// MaxWalkSpeed 를 바꾸지 않고 위치를 직접 더한다. 캐릭터 본인의 이동 위에
			// 얹히는 값이라 스프린트 등 다른 속도 변경과 서로 덮어쓰지 않는다.
			// bSweep 을 켜서 벽에 밀어 넣지 않는다.
			const FVector PushDelta = BeltDir * (RiderPushSpeed * Alpha * DeltaSeconds);
			Character->AddActorWorldOffset(PushDelta, /*bSweep=*/true);

#if !UE_BUILD_SHIPPING
			if (bDebug)
			{
				DrawDebugLine(World, GetActorLocation(), Character->GetActorLocation(),
					FColor::Green, false, 0.f, 0, 2.f);
				DrawDebugString(World, Character->GetActorLocation() + FVector(0.f, 0.f, 120.f),
					FString::Printf(TEXT("push %.0f (a=%.2f)"), RiderPushSpeed * Alpha, Alpha),
					nullptr, FColor::Green, 0.f, true);
			}
#endif
		}
	}

	// 블렌드가 다 빠졌거나 캐릭터가 사라진 항목을 정리한다.
	for (auto BlendIt = RiderBlend.CreateIterator(); BlendIt; ++BlendIt)
	{
		if (!BlendIt->Key.IsValid() || FMath::IsNearlyZero(BlendIt->Value))
		{
			BlendIt.RemoveCurrent();
		}
	}

#if !UE_BUILD_SHIPPING
	if (bDebug)
	{
		const FVector Start = GetActorLocation();
		DrawDebugDirectionalArrow(World, Start, Start + BeltDir * 300.f, 60.f,
			FColor::Yellow, false, 0.f, 0, 4.f);
		DrawDebugString(World, Start + FVector(0.f, 0.f, 200.f),
			FString::Printf(TEXT("%s  RiderPushSpeed %.0f"), *GetName(), RiderPushSpeed),
			nullptr, FColor::Yellow, 0.f, true);
	}
#endif
}

void ACSConveyorManager::BuildVisual()
{
	if (!ConveyorISM || !ConveyorMesh)
	{
		return;
	}

	// AssignedPlatforms 기준으로 유효 개수 계산(순서 유지)
	int32 ValidCount = 0;
	for (ACSConveyorPlatform* P : AssignedPlatforms)
	{
		if (P)
		{
			++ValidCount;
		}
	}

	ConveyorISM->ClearInstances();

	if (ValidCount <= 0)
	{
		ConveyorISM->MarkRenderStateDirty();
		return;
	}

	ConveyorISM->SetStaticMesh(ConveyorMesh);

	if (ConveyorMaterial)
	{
		ConveyorISM->SetMaterial(0, ConveyorMaterial);
	}

	for (int32 i = 0; i < ValidCount; ++i)
	{
		FTransform T;
		T.SetLocation(FVector(0.f, (float)i * ConveyorSpacing, 0.f));
		T.SetRotation(FQuat::Identity);
		T.SetScale3D(FVector(1.f));

		ConveyorISM->AddInstance(T);
	}

	ConveyorISM->MarkRenderStateDirty();
}

void ACSConveyorManager::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 진행값은 더 이상 복제하지 않는다. 서버 시간에서 양쪽이 각자 계산한다.
	// (ComputeProgressFromServerTime 주석 참고)
}
