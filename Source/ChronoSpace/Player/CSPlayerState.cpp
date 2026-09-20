// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CSAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Actor/System/CSRespawnPoint.h"

ACSPlayerState::ACSPlayerState()
{
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UCSAttributeSet>(TEXT("AttributeSet"));

	SetReplicates(true);
}

UAbilitySystemComponent* ACSPlayerState::GetAbilitySystemComponent() const
{
	return ASC;
}

void ACSPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (ASC)
	{
		// Health 占쏙옙화 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙占싸듸옙
		ASC->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
			.AddUObject(this, &ACSPlayerState::HealthChanged);

		ASC->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaxHealthAttribute())
			.AddUObject(this, &ACSPlayerState::MaxHealthChanged);
	}
}

void ACSPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSPlayerState, PersonalRespawnPoint);
    DOREPLIFETIME(ACSPlayerState, PlayerSlot);
}

void ACSPlayerState::OnRep_PlayerSlot()
{
    OnPlayerSlotChanged.Broadcast(PlayerSlot);
}


void ACSPlayerState::CopyProperties(APlayerState* PlayerState)
{
    Super::CopyProperties(PlayerState);

    // SeamlessTravel 은 PlayerState 를 새로 만들고 이 함수로 값을 옮긴다.
    // 여기서 안 옮기면 새 PlayerState 의 PlayerSlot 이 기본값(Player0)으로 돌아가
    // 두 명 다 1번 캐릭터로 스폰된다.
    if (ACSPlayerState* NewPS = Cast<ACSPlayerState>(PlayerState))
    {
        NewPS->PlayerSlot = PlayerSlot;
    }
}

float ACSPlayerState::GetHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth();
	}
	return 0.0f;
}

float ACSPlayerState::GetMaxHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxHealth();
	}
	return 0.0f;
}

float ACSPlayerState::GetHealthPercent() const
{
	float MaxHP = GetMaxHealth();
	if (MaxHP > 0)
	{
		return GetHealth() / MaxHP;
	}
	return 0.0f;
}

void ACSPlayerState::SetPersonalRespawnPoint(ACSRespawnPoint* NewPoint)
{
	if (HasAuthority())
	{
		PersonalRespawnPoint = NewPoint;
	}
}

ACSRespawnPoint* ACSPlayerState::GetPersonalRespawnPoint() const
{
	return PersonalRespawnPoint;
}

void ACSPlayerState::HealthChanged(const FOnAttributeChangeData& Data)
{
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

void ACSPlayerState::MaxHealthChanged(const FOnAttributeChangeData& Data)
{
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}
