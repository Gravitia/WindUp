// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CSMimicCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActorComponent/CSCharacterScaleComponent.h"
#include "ChronoSpace.h"

ACSMimicCharacter::ACSMimicCharacter()
{
	// 컨트롤러 없이 서버에서 직접 움직이는 캐릭터
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

	// PlayerState가 없으므로 자체 ASC를 가진다
	MimicASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("MimicASC"));
	MimicASC->SetIsReplicated(true);
	MimicASC->ReplicationMode = EGameplayEffectReplicationMode::Full;
	ASC = MimicASC;

	// 분신은 킬존에 반응하지 않는다
	bIgnoreKillZone = true;
}

void ACSMimicCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (MimicASC)
	{
		MimicASC->InitAbilityActorInfo(this, this);
	}
}

void ACSMimicCharacter::OnRep_PlayerState()
{
	// 부모(ACSCharacterPlayer)는 PlayerState에서 ASC를 가져오므로 건너뛴다
	APawn::OnRep_PlayerState();
}

void ACSMimicCharacter::SetDead()
{
	// 분신은 죽지 않는다 - 의도적으로 아무것도 하지 않음
}

void ACSMimicCharacter::InitMimic(ACSCharacterPlayer* InSource, const FTransform& InSourceZoneTM, const FTransform& InTargetZoneTM,
	EMimicMode InMode, float InDelaySeconds)
{
	if (!HasAuthority() || InSource == nullptr) return;

	SourceCharacter = InSource;
	MirrorQuat = InTargetZoneTM.GetRotation() * InSourceZoneTM.GetRotation().Inverse();
	MimicMode = InMode;
	DelaySeconds = FMath::Max(InDelaySeconds, 0.1f);

	CopyAbilitiesFromSource();

	// 플레이어 ASC와 동일한 조건으로 능력이 활성화되도록 태그를 맞춘다
	MimicASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement")));

	// 현재 스케일 동기화 (이후 변경은 능력 미러링으로 반영된다)
	if (ScaleComponent && InSource->ScaleComponent)
	{
		ScaleComponent->RequestScaleChange(InSource->ScaleComponent->GetCurrentScaleType());
	}
}

void ACSMimicCharacter::CopyAbilitiesFromSource()
{
	UAbilitySystemComponent* SourceASC = SourceCharacter->GetAbilitySystemComponent();
	if (SourceASC == nullptr)
	{
		UE_LOG(LogCS, Warning, TEXT("ACSMimicCharacter::CopyAbilitiesFromSource - Source ASC is null"));
		return;
	}

	// 원본에 부여된 능력을 같은 InputID로 복사 (밴 목록이 반영된 실제 부여 목록 기준)
	for (const FGameplayAbilitySpec& Spec : SourceASC->GetActivatableAbilities())
	{
		if (Spec.Ability == nullptr) continue;

		FGameplayAbilitySpec NewSpec(Spec.Ability->GetClass(), Spec.Level, Spec.InputID);
		MimicASC->GiveAbility(NewSpec);
	}
}

void ACSMimicCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !SourceCharacter.IsValid()) return;

	if (MimicMode == EMimicMode::Delayed)
	{
		TickDelayed();
	}
	else
	{
		MirrorMovement();
		MirrorJump();
	}
}

void ACSMimicCharacter::MirrorMovement()
{
	UCharacterMovementComponent* SourceMove = SourceCharacter->GetCharacterMovement();
	if (SourceMove == nullptr) return;

	ApplyMovement(SourceMove->GetCurrentAcceleration(), SourceMove->GetMaxAcceleration());
}

void ACSMimicCharacter::MirrorJump()
{
	ApplyJump(SourceCharacter->bPressedJump);
}

void ACSMimicCharacter::TickDelayed()
{
	UCharacterMovementComponent* SourceMove = SourceCharacter->GetCharacterMovement();
	if (SourceMove == nullptr) return;

	const double Now = GetWorld()->GetTimeSeconds();
	const double ReplayTime = Now - DelaySeconds;

	// 현재 입력 기록 (bPressedJump는 비트필드라 명시적 bool 변환 필요)
	InputHistory.Add({ Now, SourceMove->GetCurrentAcceleration(), SourceCharacter->bPressedJump != 0 });

	// 재생 시점보다 오래된 프레임 중 가장 최신 것만 남기고 제거
	while (InputHistory.Num() >= 2 && InputHistory[1].Time <= ReplayTime)
	{
		InputHistory.RemoveAt(0);
	}

	// DelaySeconds 이전의 이동/점프 재생 (스폰 직후에는 기록이 없어 가만히 서 있는다)
	if (InputHistory.Num() > 0 && InputHistory[0].Time <= ReplayTime)
	{
		const FMimicInputFrame& Frame = InputHistory[0];
		ApplyMovement(Frame.Accel, SourceMove->GetMaxAcceleration());
		ApplyJump(Frame.bJumpPressed);
	}

	// 능력 입력도 지연 재생
	while (PendingAbilityInputs.Num() > 0 && PendingAbilityInputs[0].Time <= ReplayTime)
	{
		ApplyAbilityInput(PendingAbilityInputs[0].InputId, PendingAbilityInputs[0].bPressed);
		PendingAbilityInputs.RemoveAt(0);
	}
}

void ACSMimicCharacter::ApplyMovement(const FVector& SourceAccel, float MaxAccelIn)
{
	if (SourceAccel.IsNearlyZero()) return;

	FVector Direction = MirrorQuat.RotateVector(SourceAccel.GetSafeNormal());
	if (MimicMode == EMimicMode::Inverted)
	{
		Direction = -Direction;
	}

	const float MaxAccel = FMath::Max(MaxAccelIn, 1.0f);
	const float InputScale = FMath::Clamp(SourceAccel.Size() / MaxAccel, 0.0f, 1.0f);

	AddMovementInput(Direction, InputScale);
}

void ACSMimicCharacter::ApplyJump(bool bSourceJumpPressed)
{
	if (bSourceJumpPressed && !bPrevJumpPressed)
	{
		Jump();
	}
	else if (!bSourceJumpPressed && bPrevJumpPressed)
	{
		StopJumping();
	}

	bPrevJumpPressed = bSourceJumpPressed;
}

void ACSMimicCharacter::MirrorAbilityInput(int32 InputId, bool bPressed)
{
	if (!HasAuthority()) return;

	// Delayed 모드는 큐에 쌓았다가 TickDelayed에서 지연 적용
	if (MimicMode == EMimicMode::Delayed)
	{
		PendingAbilityInputs.Add({ GetWorld()->GetTimeSeconds(), InputId, bPressed });
		return;
	}

	ApplyAbilityInput(InputId, bPressed);
}

void ACSMimicCharacter::ApplyAbilityInput(int32 InputId, bool bPressed)
{
	if (MimicASC == nullptr) return;

	FGameplayAbilitySpec* Spec = MimicASC->FindAbilitySpecFromInputID(InputId);
	if (Spec == nullptr) return;

	if (bPressed)
	{
		if (Spec->InputPressed) return;
		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			MimicASC->AbilitySpecInputPressed(*Spec);
		}
		else
		{
			MimicASC->TryActivateAbility(Spec->Handle);
		}
	}
	else
	{
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			MimicASC->AbilitySpecInputReleased(*Spec);
		}
	}
}
