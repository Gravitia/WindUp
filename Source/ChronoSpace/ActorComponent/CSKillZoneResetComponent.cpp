// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSKillZoneResetComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Actor/System/CSKillZone.h"

UCSKillZoneResetComponent::UCSKillZoneResetComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCSKillZoneResetComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 1) 시작 위치 저장
    StartLocation = Owner->GetActorLocation();

    // 2) Overlap 전용 BoxCollision 생성
    OverlapBox = NewObject<UBoxComponent>(Owner, TEXT("AutoOverlapBox"));
    if (!OverlapBox) return;

    OverlapBox->SetupAttachment(Owner->GetRootComponent());
    OverlapBox->RegisterComponent();

    // 3) Box 크기 (필요하면 조절)
    OverlapBox->SetBoxExtent(FVector(50.f));

    // 4) Collision 세팅 (KillZone 전용)
    OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OverlapBox->SetGenerateOverlapEvents(true);
    OverlapBox->SetCollisionProfileName(TEXT("Trigger"));

    // Pawn 등 불필요한 오버랩 제거
    OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlapBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    OverlapBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);

    // 5) Overlap 바인딩
    OverlapBox->OnComponentBeginOverlap.AddDynamic(
        this, &UCSKillZoneResetComponent::OnBeginOverlap
    );
}

void UCSKillZoneResetComponent::OnBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    AActor* Owner = GetOwner();
    if (!Owner || !OtherActor) return;

    // 서버에서만 위치 변경 (멀티 동기화 핵심)
    if (!Owner->HasAuthority())
        return;
    
    if (bIgnoreKillZone) return;

    // 킬존이 아니면 아무것도 하지 않는다.
    // (예전엔 속도 리셋이 이 검사 밖에 있어서 블랙홀 스피어, 플레이어 트리거 등 아무 오버랩에서나
    //  물리 프롭이 그 자리에 멈췄다 - 오버랩 박스가 WorldStatic/WorldDynamic 을 전부 감지한다.)
    if (!OtherActor->IsA(ACSKillZone::StaticClass()))
        return;

    Owner->SetActorLocation(
        StartLocation + RespawnOffset,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    // 되돌린 자리에서 이전 속도로 계속 날아가지 않도록 정지
    if (UPrimitiveComponent* PrimComp =
        Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
    {
        if (PrimComp->IsSimulatingPhysics())
        {
            PrimComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
            PrimComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
    }
}