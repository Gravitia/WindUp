// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_ProjectileBlackHole.h"
#include "GA/AT/CSAT_AimTrace.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "Character/CSCharacterPlayer.h"
#include "ActorComponent/CSCameraRigComponent.h"
#include "Actor/CSBlackHole.h"
#include "ChronoSpace.h"

UCSGA_ProjectileBlackHole::UCSGA_ProjectileBlackHole()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	MaxGuideDistance = 2000.0f;
	UpdateRate = 0.02f;
	MouseYSensitivity = 3.0f;

	Duration = -1.0f;	// 블랙홀 지속 시간을 주고 싶으면 양수로
	GravityInfluenceRange = 500.0f;
	PullStrength = 10.0f;
	StopRange = 100.0f;

	CameraZOffsetWhileAiming = 400.0f;
	bApplyCameraZOffsetWhileAiming = true;
}

// ---------------------------------------------------------------- 클라: 발동·조준·입력

void UCSGA_ProjectileBlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AimingAvatar = Avatar;

	AimTask = UCSAT_AimTrace::CreateAimTraceTask(this, MaxGuideDistance, MouseYSensitivity, UpdateRate);
	AimTask->OnAimUpdated.AddDynamic(this, &UCSGA_ProjectileBlackHole::OnAimUpdated);
	AimTask->ReadyForActivation();

	ApplyCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Activated"));
}

void UCSGA_ProjectileBlackHole::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 우클릭 뗌 외의 경로(사망, CancelAbility, 태그 취소)로 끝나도 서버의 블랙홀을 정리한다.
	// 안 하면 Duration 이 -1 인 블랙홀이 영원히 남는다. 플래그도 여기서 리셋해야 다음 발동이 스폰 분기로 간다.
	if (bIsBlackHoleSpawned)
	{
		SendServerCommand(FInstancedStruct::Make(FCSBlackHoleReleaseCmd()));
		bIsBlackHoleSpawned = false;
	}

	// AimTask 는 어빌리티 종료와 함께 엔진이 끝낸다. 참조만 비워 둔다.
	AimTask = nullptr;

	// 카메라 복원은 캐릭터의 UCSCameraRigComponent 가 Tick 으로 돌린다.
	// 어빌리티 타이머로 돌리면 바로 아래 Super::EndAbility 안의
	// ClearAllTimersForObject(this) 에 지워져서 복원이 한 번도 실행되지 않는다.
	RestoreCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Ended"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_ProjectileBlackHole::OnAimUpdated(FVector Direction, FVector EndLocation)
{
	// 엔진은 아바타가 죽거나 바뀌어도 EndAbility 를 부르지 않는다. 여기서 직접 끝낸다.
	// 안 끝내면 조준 Task 가 새 폰에서 계속 돌고 카메라 오프셋이 구 폰에 걸린 채 남는다.
	const ACSCharacterBase* Avatar = Cast<ACSCharacterBase>(GetAvatarActorFromActorInfo());
	if (!IsValid(Avatar) || Avatar->IsDead() || Avatar != AimingAvatar.Get())
	{
		UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole: avatar dead or changed while aiming, ending"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	CurrentDirection = Direction;
	CurrentEndLocation = EndLocation;

	CheckMouseInput();
}

void UCSGA_ProjectileBlackHole::CheckMouseInput()
{
	ACSCharacterPlayer* Character = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		return;
	}

	if ( PC->IsInputKeyDown(EKeys::RightMouseButton) )
	{
		const float Now = GetWorld()->GetTimeSeconds();

		if (!bIsBlackHoleSpawned)
		{
			FCSBlackHoleSpawnCmd Cmd;
			Cmd.Direction = CurrentDirection;
			SendServerCommand(FInstancedStruct::Make(Cmd));
			bIsBlackHoleSpawned = true;
			LastSentEndLocation = CurrentEndLocation;
			LastMoveSendTime = Now;
		}
		else
		{
			// 끝점이 1cm 이상 움직였을 때 보낸다. 방향이 같아도 캐릭터가 걸으면 끝점이 바뀐다.
			// Unreliable 이라 마지막 패킷이 드랍된 채 가만히 있으면 서버가 낡은 위치에 남으므로,
			// 변화가 없어도 MoveHeartbeatInterval 마다 한 번은 보내 복구한다.
			const bool bMoved = !CurrentEndLocation.Equals(LastSentEndLocation, 1.0f);
			const bool bHeartbeat = (Now - LastMoveSendTime) >= MoveHeartbeatInterval;
			if (bMoved || bHeartbeat)
			{
				FCSBlackHoleMoveCmd Cmd;
				Cmd.Direction = CurrentDirection;
				SendServerCommand(FInstancedStruct::Make(Cmd), /*bReliable*/ false);
				LastSentEndLocation = CurrentEndLocation;
				LastMoveSendTime = Now;
			}
		}
	}
	else
	{
		// 우클릭이 안 눌려 있으면 끝낸다. 블랙홀이 있든 없든 마찬가지다.
		// 어빌리티 키가 우클릭이라 정상 흐름에선 첫 틱에 항상 눌려 있고, 안 눌려 있다는 건 이미 뗐다는 뜻이다.
		// 리모트 클라는 활성화가 RPC 왕복 뒤라 짧은 클릭이면 여기로 온다. "스폰된 경우만" 으로 좁히면
		// 조준만 하다 멈춘 어빌리티가 영영 안 끝나고 이후 입력이 전부 거부된다.
		// 릴리즈 명령은 EndAbility 가 보낸다 (다른 종료 경로와 한 곳으로 모은다)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

// ---------------------------------------------------------------- 서버 명령 처리

bool UCSGA_ProjectileBlackHole::CanReceiveServerCommand(const FInstancedStruct& Payload) const
{
	// 릴리즈는 무조건 받는다. 죽은 뒤 우클릭을 떼는 경우가 바로 이 경로다.
	if (Payload.GetPtr<FCSBlackHoleReleaseCmd>())
	{
		return true;
	}
	return Super::CanReceiveServerCommand(Payload);
}

void UCSGA_ProjectileBlackHole::OnServerCommand(const FInstancedStruct& Payload)
{
	if (const FCSBlackHoleSpawnCmd* Spawn = Payload.GetPtr<FCSBlackHoleSpawnCmd>(); Spawn)
	{
		ServerSpawnBlackHole(Spawn->Direction);
	}
	else if (const FCSBlackHoleMoveCmd* Move = Payload.GetPtr<FCSBlackHoleMoveCmd>(); Move)
	{
		ServerMoveBlackHole(Move->Direction);
	}
	else if (Payload.GetPtr<FCSBlackHoleReleaseCmd>())
	{
		ServerReleaseBlackHole();
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("ProjectileBlackHole: unknown server command %s"),
			Payload.GetScriptStruct() ? *Payload.GetScriptStruct()->GetName() : TEXT("null"));
	}
}

void UCSGA_ProjectileBlackHole::ServerSpawnBlackHole(const FVector& Direction)
{
	ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo());
	if (!IsValid(CSPlayer) || !CSPlayer->HasAuthority())
	{
		return;
	}

	if (IsValid(SpawnedBlackHole))
	{
		// 이미 살아 있는데 스폰 명령이 또 왔다 (릴리즈 유실 등). 막지 말고 옛것을 정리한 뒤 새로 만든다.
		UE_LOG(LogCS, Warning, TEXT("ProjectileBlackHole: spawn requested while black hole already alive, replacing"));
		ServerReleaseBlackHole();
	}

	if (!BlackHoleClass)
	{
		UE_LOG(LogCS, Warning, TEXT("ProjectileBlackHole: BlackHoleClass is not set"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// 클라가 보낸 방향은 정규화만 믿고, 끝점은 서버가 다시 트레이스한다
	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		UE_LOG(LogCS, Warning, TEXT("ProjectileBlackHole: spawn direction is zero, ignored"));
		return;
	}

	const FVector SpawnLocation = UCSAT_AimTrace::TraceAimEnd(CSPlayer, SafeDirection, MaxGuideDistance);

	FActorSpawnParameters Params;
	Params.Owner = CSPlayer;
	Params.Instigator = CSPlayer;

	SpawnedBlackHole = World->SpawnActor<ACSBlackHole>(BlackHoleClass, SpawnLocation, FRotator::ZeroRotator, Params);
	if (!SpawnedBlackHole)
	{
		UE_LOG(LogCS, Warning, TEXT("ProjectileBlackHole: SpawnActor failed"));
		return;
	}

	// 클라가 릴리즈를 못 보내고 사라지는 경우(사망 리스폰, 접속 끊김) 서버가 스스로 정리한다
	CSPlayer->OnDestroyed.AddUniqueDynamic(this, &UCSGA_ProjectileBlackHole::OnServerAvatarDestroyed);
	BoundAvatar = CSPlayer;

	SpawnedBlackHole->SetDuration(Duration);
	SpawnedBlackHole->SetGravityInfluenceRange(GravityInfluenceRange);
	SpawnedBlackHole->SetPullStrength(PullStrength);
	SpawnedBlackHole->SetStopRange(StopRange);
	SpawnedBlackHole->SetCheckComponentInMesh(bCheckMeshComponentPulledByBlackHole);

	// 캐릭터의 리플리케이트 참조는 BP 호환용으로 계속 채워 준다
	CSPlayer->BlackHole = SpawnedBlackHole;
}

void UCSGA_ProjectileBlackHole::ServerMoveBlackHole(const FVector& Direction)
{
	if (!IsValid(SpawnedBlackHole) || !SpawnedBlackHole->HasAuthority())
	{
		return;
	}

	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return;
	}

	SpawnedBlackHole->SetActorLocation(UCSAT_AimTrace::TraceAimEnd(Avatar, SafeDirection, MaxGuideDistance));
}

void UCSGA_ProjectileBlackHole::ServerReleaseBlackHole()
{
	if (IsValid(SpawnedBlackHole) && SpawnedBlackHole->HasAuthority())
	{
		// SetLifeSpan(0.2) 로 0.2초 뒤 파괴. 연출은 없고 기존 동작을 그대로 옮긴 것이다.
		SpawnedBlackHole->SetDuration(0.2f);
	}
	SpawnedBlackHole = nullptr;

	// 스폰 때 바인딩·참조를 넣은 대상은 BoundAvatar 다. 리스폰으로 현재 아바타가 바뀌었을 수 있으니
	// GetAvatarActorFromActorInfo() 가 아니라 이쪽을 쓴다.
	if (AActor* Avatar = BoundAvatar.Get(); Avatar)
	{
		Avatar->OnDestroyed.RemoveDynamic(this, &UCSGA_ProjectileBlackHole::OnServerAvatarDestroyed);

		// BP 호환용 참조도 같이 비운다. 안 비우면 곧 파괴될 액터를 리플리케이트 포인터가 붙든다.
		if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(Avatar); CSPlayer)
		{
			if (CSPlayer->HasAuthority())
			{
				CSPlayer->BlackHole = nullptr;
			}
		}
	}
	BoundAvatar = nullptr;
}

void UCSGA_ProjectileBlackHole::OnServerAvatarDestroyed(AActor* DestroyedActor)
{
	if (IsValid(SpawnedBlackHole))
	{
		UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole: avatar destroyed with black hole alive, releasing"));
	}
	ServerReleaseBlackHole();
}

void UCSGA_ProjectileBlackHole::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (IsValid(SpawnedBlackHole))
	{
		UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole: ability removed with black hole alive, releasing"));
	}
	// 블랙홀이 외부에서 먼저 파괴돼 포인터만 null 이어도 OnDestroyed 바인딩·BP 참조는 남아 있을 수 있다. 조건 없이 정리한다.
	ServerReleaseBlackHole();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

// ---------------------------------------------------------------- 카메라

ACSCharacterPlayer* UCSGA_ProjectileBlackHole::GetCameraRigOwner(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* Avatar = (ActorInfo && ActorInfo->AvatarActor.IsValid())
		? ActorInfo->AvatarActor.Get()
		: GetAvatarActorFromActorInfo();

	return Cast<ACSCharacterPlayer>(Avatar);
}

void UCSGA_ProjectileBlackHole::ApplyCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bApplyCameraZOffsetWhileAiming)
	{
		return;
	}

	ACSCharacterPlayer* CSPlayer = GetCameraRigOwner(ActorInfo);
	if (!IsValid(CSPlayer))
	{
		UE_LOG(LogCS, Warning, TEXT("ApplyCameraZOffset: avatar is not ACSCharacterPlayer"));
		return;
	}

	FCSCameraModifier Modifier;
	Modifier.Source = CSCameraRigSource::BlackHoleAim;
	Modifier.BoomOffsetDelta = FVector(0.f, 0.f, CameraZOffsetWhileAiming);
	Modifier.BlendInTime = CameraOffsetLerpDuration;
	Modifier.BlendOutTime = CameraOffsetRestoreLerpDuration;
	Modifier.Blend = ECSCameraBlend::EaseInOut;

	CSPlayer->AddCameraModifier(Modifier);

	UE_LOG(LogCS, Log, TEXT("ApplyCameraZOffset: Z + %f over %f seconds"), CameraZOffsetWhileAiming, CameraOffsetLerpDuration);
}

void UCSGA_ProjectileBlackHole::RestoreCameraZOffset(const FGameplayAbilityActorInfo* ActorInfo)
{
	ACSCharacterPlayer* CSPlayer = GetCameraRigOwner(ActorInfo);
	if (!IsValid(CSPlayer))
	{
		// 여기로 빠지면 오프셋이 걸린 채 남는다. 흔적을 남겨야 추적이 된다
		UE_LOG(LogCS, Warning, TEXT("RestoreCameraZOffset: avatar is not ACSCharacterPlayer - BlackHoleAim offset left active"));
		return;
	}

	CSPlayer->RemoveCameraModifier(CSCameraRigSource::BlackHoleAim);

	UE_LOG(LogCS, Log, TEXT("RestoreCameraZOffset: release over %f seconds"), CameraOffsetRestoreLerpDuration);
}
