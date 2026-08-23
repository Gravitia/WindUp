// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/System/CSKillZone.h"
#include "Game/CSGameState.h"
#include "Game/CSGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Character/CSCharacterPlayer.h"
#include "Common/CSCommon.h"
#include "ChronoSpace.h"

ACSKillZone::ACSKillZone()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;

    // Kill Volume as root
    KillVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("KillVolume"));
    RootComponent = KillVolume;
    KillVolume->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
    KillVolume->SetCollisionProfileName("Trigger");

    // Optional visual mesh
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(RootComponent);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetVisibility(bShowVisualMesh);


}

void ACSKillZone::BeginPlay()
{
    Super::BeginPlay();

    // Trigger 바인딩
    KillVolume->OnComponentBeginOverlap.AddDynamic(
        this,
        &ACSKillZone::OnTriggerBeginOverlap
    );

    VisualMesh->SetVisibility(bShowVisualMesh);
}

void ACSKillZone::KillPlayer(APawn* Player)
{
    if (!Player || !IsValid(Player))
        return;

    // GameState에 사망 알림
    if (ACSGameState* GameState = GetCSGameState())
    {
        GameState->HandlePlayerDeath(Player);
    }

    // 캐릭터 사망 처리
    if (ACSCharacterPlayer* CharacterPlayer = Cast<ACSCharacterPlayer>(Player))
    {
        CharacterPlayer->SetDead();
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Player killed by KillZone: %s"),
        *Player->GetName()
    );
}

void ACSKillZone::SetActive(bool bNewActive)
{
    bIsActive = bNewActive;
}

void ACSKillZone::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    UE_LOG(LogTemp, Log, TEXT("CSLog : KillZone OnTriggerBeginOverlap"));

    // 사망 판정은 서버만. 클라는 캐릭터의 bIsDead 복제로 연출을 받는다.
    // (예전엔 권한 체크가 없어 클라도 로컬에서 SetDead/SetRevive 를 돌렸고, 클라 부활 후 서버 텔레포트가
    //  RTT 만큼 늦게 오면 킬존에 다시 겹쳐 이중 사망이 났다.)
    if (!HasAuthority() || !bIsActive)
        return;

    ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
    if (Player && Player->IsPlayerControlled() && !Player->bIgnoreKillZone && !Player->IsDead())
    {
        KillPlayer(Player);
        RevivePlayerWithDelay(Player, Player->GetReviveTime());
    }
}

void ACSKillZone::RevivePlayerWithDelay(APawn* Player, float DelayTime)
{
    if (!Player || !IsValid(Player))
        return;

    FTimerHandle RespawnTimer;
    TWeakObjectPtr<ACSKillZone> WeakThis = this;

    if (ACSCharacterPlayer* CharacterPlayer = Cast<ACSCharacterPlayer>(Player))
    {
        TWeakObjectPtr<ACSCharacterPlayer> WeakPlayer = CharacterPlayer;

        GetWorld()->GetTimerManager().SetTimer(
            RespawnTimer,
            [WeakThis, WeakPlayer]()
            {
                if (!WeakThis.IsValid() || !WeakPlayer.IsValid())
                    return;

                // "부활" = 구 Pawn 파괴 + 새 Pawn 스폰 (RestartPlayerAtTransform). 구 Pawn 에 SetRevive 를 부를 이유가 없다 -
                // 엔진 DisableInput 은 그 Pawn 의 InputComponent 를 PC 스택에서 pop 할 뿐이라 새 Pawn 입력과 무관하고,
                // 구 Pawn 은 같은 틱에 파괴되어 복제도 닿지 않는다. 부활 연출은 새 Pawn 의 Multicast_PlayReviveEffects 가 담당.
                ACSGameMode* GameMode = WeakThis->GetCSGameMode();
                const bool bRespawned = GameMode && GameMode->RespawnSinglePlayer(WeakPlayer.Get());

                // 리스폰 실패(리스폰 지점 없음 등)면 그 자리에서 되살려 플레이어가 영원히 숨겨진 채 남지 않게 한다
                if (!bRespawned && WeakPlayer.IsValid())
                {
                    WeakPlayer->SetRevive();
                }
            },
            DelayTime,
            false
        );
    }
}

ACSGameState* ACSKillZone::GetCSGameState() const
{
    return Cast<ACSGameState>(GetWorld()->GetGameState());
}

ACSGameMode* ACSKillZone::GetCSGameMode() const
{
    return Cast<ACSGameMode>(GetWorld()->GetAuthGameMode());
}
