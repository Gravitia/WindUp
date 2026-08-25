// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSCheckPoint.h"
#include "Actor/System/CSRespawnPoint.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/CSPlayerState.h"

ACSCheckPoint::ACSCheckPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    TriggerBox->SetCollisionProfileName("Trigger");

    bReplicates = true;

    ConnectedRespawnPoint = nullptr;
}

void ACSCheckPoint::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSCheckPoint::OnTriggerBeginOverlap);

    // 폰이 먼저 스폰되고 이 액터가 나중에 BeginPlay 하는 순서면 겹침 이벤트가 이미 지나가 버린다.
    // (레벨 스트리밍으로 늦게 올라온 서브레벨이 이 경우다.)
    if (HasAuthority())
    {
        TArray<AActor*> AlreadyInside;
        TriggerBox->GetOverlappingActors(AlreadyInside, APawn::StaticClass());
        for (AActor* Inside : AlreadyInside)
        {
            TryClaimPawn(Cast<APawn>(Inside));
        }
    }

    DebugNetworkInfo();
}

void ACSCheckPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    TryClaimPawn(Cast<APawn>(OtherActor));
}

bool ACSCheckPoint::TryClaimPawn(APawn* Pawn)
{
    if (!HasAuthority()) return false;
    if (!IsValid(Pawn) || !Pawn->IsPlayerControlled()) return false;

    ACSPlayerState* PS = Pawn->GetPlayerState<ACSPlayerState>();
    if (!PS) return false;

    // 같은 지점을 반복해서 덮어쓸 이유는 없다.
    if (PS->GetPersonalRespawnPoint() == ConnectedRespawnPoint) return false;

    PS->SetPersonalRespawnPoint(ConnectedRespawnPoint);

    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint - Personal respawn point set for %s (%s)"),
        *Pawn->GetName(), *GetNameSafe(ConnectedRespawnPoint));

    return true;
}

bool ACSCheckPoint::ClaimCheckPointAtPawnLocation(APawn* Pawn)
{
    if (!IsValid(Pawn) || !Pawn->HasAuthority()) return false;

    UWorld* World = Pawn->GetWorld();
    if (!World) return false;

    const FVector PawnLocation = Pawn->GetActorLocation();

    ACSCheckPoint* Nearest = nullptr;
    double NearestDistSq = TNumericLimits<double>::Max();

    for (TActorIterator<ACSCheckPoint> It(World); It; ++It)
    {
        ACSCheckPoint* CheckPoint = *It;
        if (!IsValid(CheckPoint) || !IsValid(CheckPoint->TriggerBox)) continue;

        // 캐시된 겹침 목록이 아니라 도형으로 직접 판정한다. 스폰 순서를 타지 않는다.
        if (!CheckPoint->TriggerBox->OverlapComponent(PawnLocation, FQuat::Identity,
            FCollisionShape::MakeSphere(1.0f)))
        {
            continue;
        }

        // 여러 개가 겹쳐 있으면 중심이 가까운 쪽이 그 구간의 체크포인트다.
        const double DistSq = FVector::DistSquared(PawnLocation, CheckPoint->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            Nearest = CheckPoint;
        }
    }

    return Nearest ? Nearest->TryClaimPawn(Pawn) : false;
}

void ACSCheckPoint::DebugNetworkInfo() const
{
    UWorld* World = GetWorld();
    if (!World) return;

    const ENetMode NetMode = World->GetNetMode();
    FString NetModeString;

    switch (NetMode)
    {
    case NM_Standalone:      NetModeString = TEXT("Standalone");      break;
    case NM_DedicatedServer: NetModeString = TEXT("DedicatedServer"); break;
    case NM_ListenServer:    NetModeString = TEXT("ListenServer");    break;
    case NM_Client:          NetModeString = TEXT("Client");          break;
    default:                 NetModeString = TEXT("Unknown");         break;
    }

    UE_LOG(LogTemp, Log, TEXT("CSLog : CheckPoint Network Mode: %s, HasAuthority: %s"),
        *NetModeString,
        HasAuthority() ? TEXT("True") : TEXT("False"));
}
