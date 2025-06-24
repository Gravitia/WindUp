// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSGASManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Player/CSPlayerState.h"

// Sets default values for this component's properties
UCSGASManagerComponent::UCSGASManagerComponent()
{
	SetIsReplicatedByDefault(true);

	ASC == nullptr;
}

void UCSGASManagerComponent::SetASC(UAbilitySystemComponent* InASC, class ACSPlayerState* PS)
{
	check(InASC != nullptr);
	check(PS != nullptr); 

	ASC = InASC;
	ASC->InitAbilityActorInfo(PS, GetOwner());
	ASC->ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UCSGASManagerComponent::SetGASAbilities()
{
	if (!GetOwner()->HasAuthority()) return;

	for (const auto& StartAbility : StartAbilities) 
	{
		FGameplayAbilitySpec StartSpec(StartAbility); 
		ASC->GiveAbility(StartSpec); 
	}

	for (const auto& StartInputAbility : StartInputAbilities) 
	{
		FGameplayAbilitySpec StartSpec(StartInputAbility.Value); 
		StartSpec.InputID = static_cast<int32>(StartInputAbility.Key); 
		ASC->GiveAbility(StartSpec); 
	} 
}

void UCSGASManagerComponent::SetupGASInputComponent(UEnhancedInputComponent* InputComponent) 
{
	ensure(InputComponent != nullptr);

	InputComponent->BindAction(ReverseGravityAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ReverseGravity));
	InputComponent->BindAction(ReverseGravityAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ReverseGravity));

	InputComponent->BindAction(BlackHoleAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::BlackHole));
	InputComponent->BindAction(BlackHoleAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::BlackHole));
	InputComponent->BindAction(WhiteHoleAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::WhiteHole));
	InputComponent->BindAction(WhiteHoleAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::WhiteHole));

	InputComponent->BindAction(WeakenGravity10PAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::WeakenGravity10P));
	InputComponent->BindAction(WeakenGravity10PAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::WeakenGravity10P));
	InputComponent->BindAction(WeakenGravity50PAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::WeakenGravity50P));
	InputComponent->BindAction(WeakenGravity50PAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::WeakenGravity50P));

	InputComponent->BindAction(ChronoControlAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ChronoControl));
	InputComponent->BindAction(ChronoControlAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ChronoControl));

	InputComponent->BindAction(TimeRewindAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::TimeRewind));
	InputComponent->BindAction(TimeRewindAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::TimeRewind));

	InputComponent->BindAction(ScaleSmallAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ScaleSmall));
	InputComponent->BindAction(ScaleSmallAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ScaleSmall));

	InputComponent->BindAction(ScaleNormalAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ScaleNormal));
	InputComponent->BindAction(ScaleNormalAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ScaleNormal));

	InputComponent->BindAction(ScaleLargeAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ScaleLarge));
	InputComponent->BindAction(ScaleLargeAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ScaleLarge));

	InputComponent->BindAction(WindUpAction, ETriggerEvent::Started,
		this, &UCSGASManagerComponent::OnWindUpStarted);
	InputComponent->BindAction(WindUpAction, ETriggerEvent::Triggered,
		this, &UCSGASManagerComponent::OnWindUpTriggered);
	InputComponent->BindAction(WindUpAction, ETriggerEvent::Completed,
		this, &UCSGASManagerComponent::OnWindUpCompleted);


	ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement")));
}

void UCSGASManagerComponent::GASInputPressed(int32 InputId)
{
	if (GetOwner()->HasAuthority())
	{
		HandleGASInputPressed(InputId);
	}
	else
	{
		ServerGASInputPressed(InputId);
	}
}

void UCSGASManagerComponent::ServerGASInputPressed_Implementation(int32 InputId)
{
	HandleGASInputPressed(InputId);
}

void UCSGASManagerComponent::HandleGASInputPressed(int32 InputId)
{
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);

	if (Spec)
	{
		if (Spec->InputPressed) return;
		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			ASC->AbilitySpecInputPressed(*Spec);
		}
		else
		{
			ASC->TryActivateAbility(Spec->Handle);
		}
	}
}

void UCSGASManagerComponent::GASInputReleased(int32 InputId)
{
	if (GetOwner()->HasAuthority())
	{
		HandleGASInputReleased(InputId);
	}
	else
	{
		ServerGASInputReleased(InputId);
	}
}

void UCSGASManagerComponent::ServerGASInputReleased_Implementation(int32 InputId)
{
	HandleGASInputReleased(InputId);
}

void UCSGASManagerComponent::HandleGASInputReleased(int32 InputId)
{
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
	if (Spec)
	{
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			ASC->AbilitySpecInputReleased(*Spec);
		}
	}
}


void UCSGASManagerComponent::OnWindUpStarted(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("WindUp Started"));

	if (!bWindUpActive)
	{
		bWindUpActive = true;

		// 어빌리티 활성화
		GASInputPressed(static_cast<int32>(EAbilityIndex::WindUp));
	}
}

void UCSGASManagerComponent::OnWindUpTriggered(const FInputActionValue& Value)
{
	// Hold 상태에서 지속적으로 호출됨
	// 필요시 추가 로직 (예: 진행도 표시)
	UE_LOG(LogTemp, Verbose, TEXT("WindUp Triggered - Holding"));
}

void UCSGASManagerComponent::OnWindUpCompleted(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("WindUp Completed"));

	if (bWindUpActive)
	{
		bWindUpActive = false;

		// 어빌리티 종료
		GASInputReleased(static_cast<int32>(EAbilityIndex::WindUp));
	}
}