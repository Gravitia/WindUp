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
		// Health 변화 델리게이트 바인딩
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
