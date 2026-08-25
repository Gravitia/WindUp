// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/System/CSRespawnPoint.h"
#include "Game/CSGameMode.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "Player/CSPlayerState.h"
#include "ChronoSpace.h"

ACSRespawnPoint::ACSRespawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // Direction Arrow as root
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    RootComponent = DirectionArrow;
    DirectionArrow->SetArrowColor(FLinearColor::Blue);
    DirectionArrow->ArrowSize = 2.0f;
}


bool ACSRespawnPoint::EnsureRespawnPoint(APawn* Pawn)
{
    if (!IsValid(Pawn) || !Pawn->HasAuthority()) return false;

    ACSPlayerState* PS = Pawn->GetPlayerState<ACSPlayerState>();
    if (!PS) return false;

    // 이미 잡혀 있으면 그대로 둔다.
    if (IsValid(PS->GetPersonalRespawnPoint())) return false;

    UWorld* World = Pawn->GetWorld();
    if (!World) return false;

    const FVector PawnLocation = Pawn->GetActorLocation();

    ACSRespawnPoint* Nearest = nullptr;
    double NearestDistSq = TNumericLimits<double>::Max();

    for (TActorIterator<ACSRespawnPoint> It(World); It; ++It)
    {
        ACSRespawnPoint* Point = *It;
        if (!IsValid(Point)) continue;

        const double DistSq = FVector::DistSquared(PawnLocation, Point->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            Nearest = Point;
        }
    }

    if (!Nearest)
    {
        // 리스폰 지점이 아직 스트리밍 안 된 서브레벨에 있을 수 있다.
        // 사망 시점에 ACSGameMode::RespawnSinglePlayer 가 한 번 더 부른다.
        UE_LOG(LogCS, Warning, TEXT("EnsureRespawnPoint: no ACSRespawnPoint found for %s"),
            *Pawn->GetName());
        return false;
    }

    PS->SetPersonalRespawnPoint(Nearest);

    UE_LOG(LogCS, Log, TEXT("EnsureRespawnPoint: fallback %s for %s (%.0fm)"),
        *Nearest->GetName(), *Pawn->GetName(), FMath::Sqrt(NearestDistSq) * 0.01);

    return true;
}
