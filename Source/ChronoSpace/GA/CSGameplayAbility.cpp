// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGameplayAbility.h"
#include "Character/CSCharacterPlayer.h"
#include "ChronoSpace.h"

bool UCSGameplayAbility::CanReceiveServerCommand(const FInstancedStruct& Payload) const
{
	const ACSCharacterBase* Avatar = Cast<ACSCharacterBase>(GetAvatarActorFromActorInfo());
	return IsValid(Avatar) && !Avatar->IsDead();
}

void UCSGameplayAbility::SendServerCommand(const FInstancedStruct& Payload, bool bReliable)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		UE_LOG(LogCS, Warning, TEXT("%s: SendServerCommand - avatar is invalid, command dropped"), *GetName());
		return;
	}

	// 리슨 호스트: 이 인스턴스가 곧 서버 인스턴스다. RPC 를 거칠 필요가 없다.
	// 단 게이트는 RPC 경로(DispatchAbilityCommand)와 똑같이 거쳐야 호스트만 검사를 피하는 구멍이 안 생긴다.
	if (Avatar->HasAuthority())
	{
		if (!CanReceiveServerCommand(Payload))
		{
			// 죽은 채 우클릭을 잡고 있으면 이동 명령이 매 틱 거부된다. Log 로 두면 다른 로그가 묻힌다.
			UE_LOG(LogCS, Verbose, TEXT("%s: SendServerCommand - rejected locally (avatar dead or ability gate)"), *GetName());
			return;
		}
		OnServerCommand(Payload);
		return;
	}

	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(Avatar);
	if (!CSPlayer)
	{
		UE_LOG(LogCS, Warning, TEXT("%s: SendServerCommand - avatar %s is not ACSCharacterPlayer, command dropped"), *GetName(), *Avatar->GetName());
		return;
	}

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	if (bReliable)
	{
		CSPlayer->ServerAbilityCommand(Handle, Payload);
	}
	else
	{
		CSPlayer->ServerAbilityCommandUnreliable(Handle, Payload);
	}
}
