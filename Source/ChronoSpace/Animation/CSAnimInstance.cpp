// Fill out your copyright notice in the Description page of Project Settings.

#include "CSAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActorComponent/CSCustomGravityDirComponent.h"

UCSAnimInstance::UCSAnimInstance()
{
	MovingThreshold = 3.0f;
	JumpingThreshold = 100.0f;
}

void UCSAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Owner = Cast<ACharacter>(GetOwningActor());

	if ( Owner )
	{
		Movement = Owner->GetCharacterMovement();
		GravityComponent = Owner->GetComponentByClass<UCSCustomGravityDirComponent>();
	}
}

void UCSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if ( Movement )
	{
		Velocity = Movement->Velocity; 
		GroundSpeed = Velocity.Size2D(); 
		bIsIdle = GroundSpeed < MovingThreshold;
		bIsFalling = Movement->IsFalling(); 
		bIsJumping = bIsFalling & (Velocity.Z > JumpingThreshold); 
	}

	/* 화면 디버그 출력 ---------------------------
	if (GEngine)

	{
		GEngine->AddOnScreenDebugMessage(
			1,                            // 고정 ID → 같은 줄에 덮어쓰기
			0.f,                          // 지속시간 0 = 매 프레임 갱신
			FColor::Yellow,               // 색상
			FString::Printf(TEXT("GroundSpeed: %.2f"), GroundSpeed)
		);
	}
	*/

	if ( GravityComponent )
	{
		bIsGravityCustomized = GravityComponent->IsGravityCustomzied();
	}
}
