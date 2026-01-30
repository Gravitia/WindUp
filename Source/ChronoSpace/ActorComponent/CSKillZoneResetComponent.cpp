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

    if (OtherActor->IsA(ACSKillZone::StaticClass()))
    {
        Owner->SetActorLocation(
            StartLocation + RespawnOffset,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }

    // 1) RootComponent가 물리 시뮬레이션 중이면
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