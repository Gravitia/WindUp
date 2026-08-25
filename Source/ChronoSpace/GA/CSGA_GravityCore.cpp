// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_GravityCore.h"
#include "Actor/CSGravityCoreSphere.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"
#include "Character/CSCharacterPlayer.h"

UCSGA_GravityCore::UCSGA_GravityCore()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	GravityCoreClass = nullptr;
}

void UCSGA_GravityCore::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	check(GravityCoreClass);

	UE_LOG(LogCS, Log, TEXT("[NetMode: %d] UCSGA_GravityCore - ActivateAbility"), GetWorld()->GetNetMode());

	// 어빌리티 인스턴스는 PlayerState 의 ASC 에 살아서 Pawn 보다 오래 산다.
	// 죽어서 코어가 파괴되면 이 포인터는 GC 전까지 파괴 대기 상태로 남으므로 nullptr 비교로는 걸러지지 않는다
	// (그래서 부활 직후 첫 입력이 코어를 켜지 않고 OffCore 로 빠졌다).
	if (!IsValid(GravityCore))
	{
		GravityCore = nullptr;
		OnCore();
	}
	else
	{
		OffCore();
	}
}

void UCSGA_GravityCore::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_GravityCore::OnCore()
{
	OwnerCharacter = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
	if (!OwnerCharacter) return;
	
	// 기존 On 사운드가 남아있다면 정리 (캐릭터에 붙어 있으므로 사망 시 함께 파괴됐을 수 있다)
	if (IsValid(GravityCoreOnAudioComp))
	{
		GravityCoreOnAudioComp->Stop();
	}
	GravityCoreOnAudioComp = nullptr;

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		FVector SpawnLocation = OwnerCharacter->GetActorLocation();

		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnTransform.SetRotation(FQuat::Identity);
		SpawnTransform.SetScale3D(FVector(CoreScale));

		GravityCore = GetWorld()->SpawnActorDeferred<ACSGravityCoreSphere>(GravityCoreClass, SpawnTransform, OwnerCharacter, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (GravityCore)
		{
			GravityCore->SetCheckComponentInMesh(bCheckMeshComponentAffectedByGravityCore);
			UGameplayStatics::FinishSpawningActor(GravityCore, SpawnTransform);
			GravityCore->AttachToActor(CurrentActorInfo->AvatarActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}

		if (GravityCoreOnSound)
		{
			GravityCoreOnAudioComp =
				UGameplayStatics::SpawnSoundAttached(
					GravityCoreOnSound,
					OwnerCharacter->GetRootComponent(),
					NAME_None,
					FVector::ZeroVector,
					EAttachLocation::SnapToTarget,
					false
				);
		}

		if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(OwnerCharacter))
		{
			CSCharacter->NetMulticastMakeGravityCoreSphere(GravityCore->GetMeshRadius(), GravityCore->GetActorScale3D().X);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCSGA_GravityCore::OffCore()
{
	// On 사운드 강제 종료
	if (IsValid(GravityCoreOnAudioComp))
	{
		GravityCoreOnAudioComp->Stop();
	}
	GravityCoreOnAudioComp = nullptr;

	if (IsValid(GravityCore))
	{
		if (GravityCoreOffSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				GravityCoreOffSound,
				GravityCore->GetActorLocation()
			);
		}

		GravityCore->Destroy();
	}
	GravityCore = nullptr;

	// 캐시된 OwnerCharacter 는 리스폰 후 파괴된 구 Pawn 을 가리킬 수 있다 - 현재 아바타를 쓴다
	if (ACSCharacterPlayer* CSCharacter = Cast<ACSCharacterPlayer>(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr))
	{
		CSCharacter->NetMulticastDestroyGravityCoreSphere();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}