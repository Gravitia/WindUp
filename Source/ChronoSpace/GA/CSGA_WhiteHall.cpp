// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_WhiteHall.h"
#include "Actor/CSWhiteHall.h"
#include "Character/CSCharacterPlayer.h"
#include "GameFramework/PlayerState.h"
#include "ChronoSpace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UCSGA_WhiteHall::UCSGA_WhiteHall()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	WhiteHallClass = nullptr;
}

void UCSGA_WhiteHall::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//UE_LOG(LogCS, Log, TEXT("[NetMode : %d] UCSGA_WhiteHall::ActivateAbility, %s, %s"), GetWorld()->GetNetMode(), *ActorInfo->AvatarActor.Get()->GetName(), *(Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor.Get())->GetPlayerState()->GetName()));

	bool bReplicatedEndAbility = true;
	bool bWasCancelled = false;

	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(ActorInfo->AvatarActor);
	if ( Player->GetWhiteHall() )
	{
		Player->ClearWhiteHall(); 
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled); 
		return;
	}

	if (WhiteHallClass)
	{
		FVector SpawnLocation = Player->GetActorLocation();

		ACSWhiteHall* WhiteHall = GetWorld()->SpawnActor<ACSWhiteHall>(WhiteHallClass, SpawnLocation, FRotator::ZeroRotator);
		Player->SetWhiteHall(WhiteHall);
	}



	ACSCharacterBase* Character = Cast<ACSCharacterBase>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (Character && ASC && WhiteHallMontage)
	{
		FScopedPredictionWindow ScopedPrediction(ASC, !Character->HasAuthority());

		// 로컬 예측 재생
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("WhiteHallMontage"),
			WhiteHallMontage,
			1.0f,
			NAME_None,
			true);

		MontageTask->OnCompleted.AddDynamic(this, &UCSGA_WhiteHall::OnMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UCSGA_WhiteHall::OnMontageInterrupted);

		MontageTask->ReadyForActivation();

		//  다른 클라에게도 애니메이션 전파
		if (Character&&ASC)
		{
			Character->NetMulticastPlayOtherClientMontage(WhiteHallMontage);
		}
	}

	//EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UCSGA_WhiteHall::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCSGA_WhiteHall::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}