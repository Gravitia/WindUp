// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSConveyorPlatform.h"
#include "Actor/CSConveyorManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

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
	Manager = InManager;
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

        // 이 플랫폼을 movement base(밟고 있는 바닥)로 삼는 캐릭터만 대상으로 한다.
        const UPrimitiveComponent* Base = Character->GetMovementBase();
        if (!Base || Base->GetOwner() != this)
        {
            continue;
        }

        // 권한이 있는 서버, 또는 그 캐릭터를 직접 조종하는 머신에서만 처리한다.
        // 다른 클라이언트의 시뮬레이션 프록시는 서버의 결과가 복제되어 따라온다.
        if (!Character->HasAuthority() && !Character->IsLocallyControlled())
        {
            continue;
        }

        // base를 끊으면 플랫폼의 wrap 이동량이 캐릭터에게 전달되지 않아 제자리에 남는다.
        Character->SetBase(nullptr);

        if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
        {
            // 다음 업데이트에서 즉시 바닥을 다시 검사 -> 컨베이어 출구 지면에 자연스럽게
            // 서거나, 지면이 없으면 떨어지게 한다.
            CMC->bForceNextFloorCheck = true;
        }

        // 서버에서 base 변경을 즉시 복제해, 다른 클라이언트에서 라이더가
        // wrap하는 플랫폼을 따라 잠깐 끌려가 보이는 현상을 최소화한다.
        if (Character->HasAuthority())
        {
            Character->ForceNetUpdate();
        }
    }
}
