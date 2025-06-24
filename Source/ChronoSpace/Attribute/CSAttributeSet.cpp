// Fill out your copyright notice in the Description page of Project Settings.


#include "Attribute/CSAttributeSet.h"
#include "ChronoSpace.h"
#include "GameplayEffectExtension.h"
#include "Player/CSPlayerController.h"
#include "Net/UnrealNetwork.h"

UCSAttributeSet::UCSAttributeSet() : MaxHealth(100.0f), Damage(0.0f)
{
	InitHealth(GetMaxHealth());

}

void UCSAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UCSAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCSAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UCSAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCSAttributeSet, Health, OldHealth);
}

void UCSAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCSAttributeSet, MaxHealth, OldMaxHealth);
}

void UCSAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetDamageAttribute())
	{
		NewValue = NewValue < 0.0f ? 0.0f : NewValue;
	}

}

void UCSAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	float MinimumHealth = 0.0f;

	// HP Å¬·¥ÇÎ
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), MinimumHealth, GetMaxHealth()));
	}

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth() - GetDamage(), MinimumHealth, GetMaxHealth()));

		UE_LOG(LogTemp, Warning, TEXT(" Damage Detected : %f | Now Energy : %f"), GetDamage(), GetHealth());

		AActor* TargetActor = Data.Target.GetAvatarActor();
		if (TargetActor == nullptr) return;

		if (APawn* Pawn = Cast<APawn>(TargetActor))
		{
			ACSPlayerController* PC = Cast<ACSPlayerController>(Pawn->GetController());

			if (PC)
			{
				PC->ShakeCamera();
			}
		}
	}
}
