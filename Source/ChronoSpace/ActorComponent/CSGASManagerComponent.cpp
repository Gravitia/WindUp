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

	ASC = nullptr;
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
	if (!GetOwner()->HasAuthority() || !ASC) return;

	// ASC 는 PlayerState 에 살고 Pawn 보다 오래 산다. PossessedBy 는 리스폰마다 다시 오므로
	// 이미 부여된 클래스는 건너뛴다 (예전엔 리스폰할 때마다 스펙이 한 벌씩 누적됐다).
	for (const auto& StartAbility : StartAbilities) 
	{
		if (!StartAbility || ASC->FindAbilitySpecFromClass(StartAbility)) continue;

		FGameplayAbilitySpec StartSpec(StartAbility); 
		ASC->GiveAbility(StartSpec); 
	}

	for (const auto& StartInputAbility : StartInputAbilities) 
	{
		if ( UE_BUILD_SHIPPING )
		{
			TSet< EAbilityIndex > BannedAbilities;
			if (Cast<APawn>(GetOwner())->IsLocallyControlled()) // ����, ���ʿ� SetGASAbilities ��ü�� ������ �����
				BannedAbilities = AbilitiesPlayer1Banned;
			else
				BannedAbilities = AbilitiesPlayer2Banned;

			if (BannedAbilities.Find(StartInputAbility.Key))
				continue;
		}

		if (!StartInputAbility.Value || ASC->FindAbilitySpecFromClass(StartInputAbility.Value)) continue;

		FGameplayAbilitySpec StartSpec(StartInputAbility.Value); 
		StartSpec.InputID = static_cast<int32>(StartInputAbility.Key); 
		ASC->GiveAbility(StartSpec); 
	} 
}

void UCSGASManagerComponent::SetupGASInputComponent(UEnhancedInputComponent* InputComponent) 
{
	if (InputComponent == nullptr) return;

	// 클라에서는 SetupPlayerInputComponent 와 OnRep_PlayerState 가 순서 보장 없이 둘 다 여기로 온다.
	// 같은 InputComponent 에 두 번 바인딩하면 키 한 번에 Server RPC 가 2발 나가고 루즈 태그가 2번 쌓인다.
	if (BoundInputComponent.Get() == InputComponent) return;
	BoundInputComponent = InputComponent;

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

	InputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::Sprint));
	InputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::Sprint));

	InputComponent->BindAction(GravityCoreAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::GravityCore));
	InputComponent->BindAction(GravityCoreAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::GravityCore));

	InputComponent->BindAction(ProjectileBlackHoleAction, ETriggerEvent::Triggered, this, &UCSGASManagerComponent::GASInputPressed, static_cast<int32>(EAbilityIndex::ProjectileBlackHole)); 
	InputComponent->BindAction(ProjectileBlackHoleAction, ETriggerEvent::Completed, this, &UCSGASManagerComponent::GASInputReleased, static_cast<int32>(EAbilityIndex::ProjectileBlackHole)); 

	if ( ASC && !ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement"))) )
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement")));
	}
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
	// 서버에서만 호출되는 지점 - 분신 미러링 등 구독자에게 알림
	OnServerGASInput.Broadcast(InputId, true);

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
	// 서버에서만 호출되는 지점 - 분신 미러링 등 구독자에게 알림
	OnServerGASInput.Broadcast(InputId, false);

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

		// �����Ƽ Ȱ��ȭ
		GASInputPressed(static_cast<int32>(EAbilityIndex::WindUp));
	}
}

void UCSGASManagerComponent::OnWindUpTriggered(const FInputActionValue& Value)
{
	// Hold ���¿��� ���������� ȣ���
	// �ʿ�� �߰� ���� (��: ���൵ ǥ��)
	UE_LOG(LogTemp, Verbose, TEXT("WindUp Triggered - Holding"));
}

void UCSGASManagerComponent::OnWindUpCompleted(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("WindUp Completed"));

	if (bWindUpActive)
	{
		bWindUpActive = false;

		// �����Ƽ ����
		GASInputReleased(static_cast<int32>(EAbilityIndex::WindUp));
	}
}