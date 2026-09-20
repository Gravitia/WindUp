// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorPlatform.h"
#include "Actor/CSConveyorManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "ChronoSpace.h"

ACSConveyorPlatform::ACSConveyorPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	SetReplicateMovement(false);
	bReplicates = true;


}

void ACSConveyorPlatform::SetManager(ACSConveyorManager* InManager)
{
	if (Manager == InManager)
	{
		return;
	}

	// 매니저가 바뀌면 예전 의존부터 푼다. 안 풀면 죽은 의존이 남는다.
	if (Manager)
	{
		RemoveTickPrerequisiteActor(Manager);
	}

	Manager = InManager;

	// 이 플랫폼이 항상 매니저 "다음에" 돌도록 묶는다.
	//
	// 플랫폼은 Tick 에서 Manager->GetSmoothedProgress() 를 읽는데 액터 틱 순서는
	// 보장되지 않는다. 묶지 않으면 어떤 플랫폼은 이번 프레임 값을, 어떤 플랫폼은 지난
	// 프레임 값을 읽어 플레이트 간격이 한 프레임 이동량만큼 어긋난 채로 굳는다.
	//
	// 더 중요한 건 그 순서가 서버와 클라에서 다를 수 있다는 점이다. 진행값을 서버 시간에서
	// 계산해 양쪽을 맞춰 놔도, 읽는 시점이 어긋나면 플레이트 위치가 다시 벌어진다.
	if (Manager)
	{
		AddTickPrerequisiteActor(Manager);
	}
}

void ACSConveyorPlatform::SetIndexOffset(float InOffsetDistance)
{
	OffsetDistance = InOffsetDistance;
}

void ACSConveyorPlatform::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Manager)
    {
        return;
    }

    const float TotalLength = Manager->GetTotalLength();
    if (TotalLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    float D = OffsetDistance + Manager->GetSmoothedProgress();
    D = FMath::Fmod(D, TotalLength);
    if (D < 0.f)
    {
        D += TotalLength;
    }

    // 이번 프레임에 이 플랫폼이 끝 -> 시작점으로 wrap(순간이동) 했는지 감지한다.
    // 평소엔 D가 MoveSpeed*Dt 만큼만 변하지만, wrap 시엔 거의 TotalLength 만큼 점프한다.
    // (Manager가 progress 스냅을 판단할 때 쓰는 TotalLength*0.5 기준과 동일.)
    const bool bWrappedThisFrame =
        bHasPrevD && (FMath::Abs(D - PrevD) > TotalLength * 0.5f);

    PrevD = D;
    bHasPrevD = true;

    // wrap 하는 그 순간, 이 플랫폼을 밟고 있던 캐릭터를 떼어내
    // 플랫폼과 함께 텔레포트되는 것을 막는다. (플랫폼이 이동하기 "전에" 끊어야 한다.)
    if (bWrappedThisFrame)
    {
        DetachRidersOnWrap();
    }

    // Use manager's transform to calculate world position and rotation
    FVector LocalOffset(0.f, D, ZOffset);
    FTransform TargetTransform = Manager->GetActorTransform();

    // Position: Manager Location + (Manager Rotation * LocalOffset)
    FVector WorldLoc = TargetTransform.TransformPosition(LocalOffset);

    // Update both location and rotation
    SetActorLocationAndRotation(WorldLoc, TargetTransform.GetRotation(), false, nullptr, ETeleportType::None);
}

void ACSConveyorPlatform::DetachRidersOnWrap()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<ACharacter> It(World); It; ++It)
    {
        ACharacter* Character = *It;
        if (!IsValid(Character))
        {
            continue;
        }

        // 이 플랫폼을 movement base(밟고 있는 바닥)로 삼는 캐릭터만 대상으로 한다.
        const UPrimitiveComponent* Base = Character->GetMovementBase();
        if (!Base || Base->GetOwner() != this)
        {
            continue;
        }

        // 머신을 가리지 않고 올라탄 캐릭터를 전부 끊는다.
        //
        // 예전에는 권한이 있거나 직접 조종하는 캐릭터만 끊고, 다른 클라이언트의
        // 시뮬레이션 프록시는 "서버 결과가 복제되어 따라온다" 며 건너뛰었다. 그게 틀렸다.
        // 프록시의 위치는 발판 기준 *상대 좌표* 로 복제된다. 발판이 wrap 으로
        // 순간이동하면 프록시는 그 프레임에 같이 날아가고, 서버의 정정이 도착할 때까지
        // 그 상태로 보인다. 그래서 클라이언트 화면에서 상대 캐릭터만 벨트 끝에서
        // 튕겨 나가는 것처럼 보였다.
        //
        // SetBase 는 위치를 바꾸지 않고 따라가기만 멈춘다. 프록시에 로컬로 걸어도
        // 다음 복제가 오면 서버 값으로 덮어써지므로 권한을 침범하지 않는다.
        // base를 끊으면 플랫폼의 wrap 이동량이 캐릭터에게 전달되지 않아 제자리에 남는다.
		Character->SetBase(static_cast<UPrimitiveComponent*>(nullptr));

        if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
        {
            // 다음 업데이트에서 즉시 바닥을 다시 검사 -> 컨베이어 출구 지면에 자연스럽게
            // 서거나, 지면이 없으면 떨어지게 한다.
            CMC->bForceNextFloorCheck = true;
        }

        // 어느 머신에서 어떤 역할의 캐릭터를 끊었는지 남긴다.
        // Authority=0, Local=0 이면 시뮬레이션 프록시다. 예전 코드가 건너뛰어
        // 벨트 끝에서 상대 캐릭터만 튕겨 나가게 만들던 바로 그 경우다.
        UE_LOG(LogCS, Verbose,
            TEXT("ConveyorPlatform %s: wrap detach %s (Authority=%d Local=%d)"),
            *GetName(), *Character->GetName(),
            Character->HasAuthority() ? 1 : 0,
            Character->IsLocallyControlled() ? 1 : 0);

        // 서버에서 base 변경을 즉시 복제해, 다른 클라이언트에서 라이더가
        // wrap하는 플랫폼을 따라 잠깐 끌려가 보이는 현상을 최소화한다.
        if (Character->HasAuthority())
        {
            Character->ForceNetUpdate();
        }
    }
}
