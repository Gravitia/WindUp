// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/CSGA_ProjectileBlackHole.h"
#include "GA/CSGA_BlackHole.h"
#include "GA/CSGA_CameraZoom.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Character/CSCharacterPlayer.h"
#include "ActorComponent/CSCameraRigComponent.h"
#include "Actor/CSBlackHoleDummy.h"
#include "Actor/CSBlackHole.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "ChronoSpace.h"

UCSGA_ProjectileBlackHole::UCSGA_ProjectileBlackHole()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	GuideDuration = 5.0f;
	MaxGuideDistance = 2000.0f;
	UpdateRate = 0.02f;
	MouseYSensitivity = 3.0f;

	CurrentEndLocation = FVector::ZeroVector;

	//bIsDummySpawned = false;
	bIsBlackHoleSpawned = false;
	bIsAming = false;

	Duration = -1.0f;	// 블랙홀 지속 시간을 주고 싶으면 양수로
	GravityInfluenceRange = 500.0f;
	PullStrength = 10.0f;
	StopRange = 100.0f;

	CameraZOffsetWhileAiming = 400.0f;
	bApplyCameraZOffsetWhileAiming = true;

	bRetriggerInstancedAbility = true;
}

void UCSGA_ProjectileBlackHole::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// GAS 컴포넌트 구조 상 서버는 이미 눌렸을 때 ActivateAbility 발동 안함
	// 클라 토글용 코드
	if ( bIsAming && !bIsBlackHoleSpawned )
	{
		bIsAming = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bIsAming = true;

	//bIsDummySpawned = false;

	if (!GetAvatarActorFromActorInfo())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 초기 상태 설정
	bUsingMouseAiming = false;
	LastMousePosition = FVector2D::ZeroVector;
	bInitialDirectionSet = false;

	// 초기 조준 방향 저장 (한 번만 설정)
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		InitialAimDirection = Character->GetActorForwardVector();
		bInitialDirectionSet = true;
		UE_LOG(LogCS, Log, TEXT("Initial aim direction set: %s"), *InitialAimDirection.ToString());

		// 초기 마우스 위치 저장
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			float MouseX, MouseY;
			if (PC->GetMousePosition(MouseX, MouseY))
			{
				LastMousePosition = FVector2D(MouseX, MouseY);
			}
		}
	}

	// 업데이트 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		UpdateTimerHandle,
		this,
		&UCSGA_ProjectileBlackHole::UpdateGuideLine,
		UpdateRate,
		true
	);
	/*
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (CameraZoomAbilityClass)
		{
			ASC->TryActivateAbilityByClass(CameraZoomAbilityClass);
		}
	}
	*/

	ApplyCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Activated"));

}

FVector UCSGA_ProjectileBlackHole::GetScreenCenterDirection() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			// Determine which split-screen slot this player is in:
			// 0 = left/top, 1 = right/bottom (for two players)
			int32 ControllerId = PC->GetLocalPlayer()->GetControllerId();

			// Left player uses 75% X (right side), right player 25% X (left side)
			float ScreenCenterX = (ControllerId == 0)
				? ViewportSizeX * 0.75f
				: ViewportSizeX * 0.25f;

			float ScreenCenterY = ViewportSizeY * 0.5f;

			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				// apply Y-axis sensitivity as before
				float MouseYOffset = CurrentMouseY - ScreenCenterY;
				float AmplifiedYOffset = MouseYOffset * MouseYSensitivity;
				float FinalY = ScreenCenterY + AmplifiedYOffset;

				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectScreenPositionToWorld(ScreenCenterX, FinalY, WorldLocation, WorldDirection))
				{
					return WorldDirection;
				}
			}
		}
	}

	return bInitialDirectionSet ? InitialAimDirection : FVector::ForwardVector;
}

void UCSGA_ProjectileBlackHole::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 우클릭 뗌 외의 경로(사망, CancelAbility, 태그 취소, 재발동)로 끝나도 서버의 블랙홀을 정리한다.
	// 안 하면 Duration 이 -1 인 블랙홀이 영원히 남는다. 플래그도 여기서 리셋해야 다음 발동이 스폰 분기로 간다.
	if (bIsBlackHoleSpawned)
	{
		SendServerCommand(FInstancedStruct::Make(FCSBlackHoleReleaseCmd()));
		bIsBlackHoleSpawned = false;
	}

	if ( BlackHoleDummyActor )
	{
		BlackHoleDummyActor->Destroy();
	}

	// 타이머 정리
	if (UpdateTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	if (DurationTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	// 카메라 복원은 캐릭터의 UCSCameraRigComponent 가 Tick 으로 돌린다.
	// 어빌리티 타이머로 돌리면 바로 아래 Super::EndAbility 안의
	// ClearAllTimersForObject(this) 에 지워져서 복원이 한 번도 실행되지 않는다.
	RestoreCameraZOffset(ActorInfo);

	UE_LOG(LogCS, Log, TEXT("ProjectileBlackHole Ended"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCSGA_ProjectileBlackHole::UpdateGuideLine()
{
	// 마우스 이동 감지
	CheckMouseMovement();

	CurrentDirection = GetScreenCenterDirection();

	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection = FVector::ForwardVector;
	}

	CurrentEndLocation = TraceAimEndLocation(CurrentDirection);

	CheckMouseInput();
}

void UCSGA_ProjectileBlackHole::OnGuideDurationEnd()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}


void UCSGA_ProjectileBlackHole::CheckMouseInput()
{
	if (ACSCharacterPlayer* Character = Cast<ACSCharacterPlayer>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
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
			else if (bIsBlackHoleSpawned)
			{
				// 릴리즈 명령은 EndAbility 가 보낸다 (다른 종료 경로와 한 곳으로 모은다)
				bIsAming = false;

				/*
				if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
				{
					if (CameraZoomAbilityClass)
					{
						if (FGameplayAbilitySpec* Spec =
							ASC->FindAbilitySpecFromClass(CameraZoomAbilityClass))
						{
							ASC->CancelAbilityHandle(Spec->Handle);
						}
					}
				}
				*/

				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}
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
	if (const FCSBlackHoleSpawnCmd* Spawn = Payload.GetPtr<FCSBlackHoleSpawnCmd>())
	{
		ServerSpawnBlackHole(Spawn->Direction);
	}
	else if (const FCSBlackHoleMoveCmd* Move = Payload.GetPtr<FCSBlackHoleMoveCmd>())
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

	const FVector SpawnLocation = TraceAimEndLocation(SafeDirection);

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

	SpawnedBlackHole->SetActorLocation(TraceAimEndLocation(SafeDirection));
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
	if (AActor* Avatar = BoundAvatar.Get())
	{
		Avatar->OnDestroyed.RemoveDynamic(this, &UCSGA_ProjectileBlackHole::OnServerAvatarDestroyed);

		// BP 호환용 참조도 같이 비운다. 안 비우면 곧 파괴될 액터를 리플리케이트 포인터가 붙든다.
		if (ACSCharacterPlayer* CSPlayer = Cast<ACSCharacterPlayer>(Avatar))
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

FVector UCSGA_ProjectileBlackHole::TraceAimEndLocation(const FVector& Direction) const
{
	const FVector StartLocation = GetStartLocation();
	FVector EndLocation = StartLocation + Direction * MaxGuideDistance;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return EndLocation;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	if (UCSManagedActorSubsystem* Subsystem = World->GetSubsystem<UCSManagedActorSubsystem>())
	{
		QueryParams.AddIgnoredActors(Subsystem->GetActorsPulledByBlackHole());
	}

	FHitResult HitResult;
	if (World->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		EndLocation = HitResult.Location;
	}

	return EndLocation;
}

void UCSGA_ProjectileBlackHole::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// GAS 컴포넌트 구조상 서버에서만 불린다
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if ( Character == nullptr ) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if ( PC == nullptr ) return;

	// 이미 소환한 후에는 왼쪽 버튼 놔줄 때로 종료 체크
	if ( !bIsBlackHoleSpawned )
	{
		bIsAming = false;

		/*
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (CameraZoomAbilityClass)
			{
				if (FGameplayAbilitySpec* Spec =
					ASC->FindAbilitySpecFromClass(CameraZoomAbilityClass))
				{
					ASC->CancelAbilityHandle(Spec->Handle);
				}
			}
		}
		*/

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}


FVector UCSGA_ProjectileBlackHole::GetStartLocation() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		return Character->GetActorLocation() + FVector(0.0f, 0.0f, Character->BaseEyeHeight);
	}

	return FVector::ZeroVector;
}

void UCSGA_ProjectileBlackHole::CheckMouseMovement()
{
	if (bUsingMouseAiming) return; // 이미 마우스 모드면 체크하지 않음

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			float CurrentMouseX, CurrentMouseY;
			if (PC->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				FVector2D CurrentMousePosition(CurrentMouseX, CurrentMouseY);

				// 마우스가 임계값 이상 움직였는지 확인
				float MouseDistance = FVector2D::Distance(LastMousePosition, CurrentMousePosition);

				if (MouseDistance > MouseMovementThreshold)
				{
					bUsingMouseAiming = true;
					UE_LOG(LogCS, Log, TEXT("Switched to mouse aiming mode (Distance: %f)"), MouseDistance);
				}

				LastMousePosition = CurrentMousePosition;
			}
		}
	}
}


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
