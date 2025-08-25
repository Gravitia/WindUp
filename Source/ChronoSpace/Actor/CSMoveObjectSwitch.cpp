// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMoveObjectSwitch.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

ACSMoveObjectSwitch::ACSMoveObjectSwitch()
{
	// Tick 활성화
	PrimaryActorTick.bCanEverTick = true;

	// 기본값 설정
	MoveSpeed = 200.0f;           // 200 유닛/초
	RotationSpeed = 90.0f;        // 90도/초
	MoveTolerance = 5.0f;         // 5 유닛 허용 오차
	RotationTolerance = 1.0f;     // 1도 허용 오차
	bIsMoving = false;
	bMovingToTarget = false;

	// 네트워크 복제 설정
	bReplicates = true;
	SetReplicateMovement(true);
}

void ACSMoveObjectSwitch::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 초기 위치/회전 설정
	if (HasAuthority())
	{
		InitializeObjectPositions();
		CalculateTargetTransforms();
	}
}

void ACSMoveObjectSwitch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서만 이동 로직 실행
	if (HasAuthority() && bIsMoving)
	{
		UpdateMovement(DeltaTime);
	}
}

void ACSMoveObjectSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSMoveObjectSwitch, bIsMoving);
	DOREPLIFETIME(ACSMoveObjectSwitch, bMovingToTarget);
	DOREPLIFETIME(ACSMoveObjectSwitch, MoveObjects);
}

void ACSMoveObjectSwitch::Interact()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogCS, Log, TEXT("[NetMode: %d] Interact MoveObjectSwitch - Current State: %s"),
		GetWorld()->GetNetMode(), bIsInteracted ? TEXT("ON") : TEXT("OFF"));

	// 부모 클래스의 상호작용 (스위치 상태 변경)
	Super::Interact();

	// 이동 시작
	StartMovement();
}

void ACSMoveObjectSwitch::SetInteracted(bool bInInteracted)
{
	if (!HasAuthority())
	{
		return;
	}

	bool bStateChanged = (bIsInteracted != bInInteracted);
	bIsInteracted = bInInteracted;
	SetMaterial();

	// 상태가 변경된 경우에만 이동 시작
	if (bStateChanged)
	{
		StartMovement();
	}
}

void ACSMoveObjectSwitch::InitializeObjectPositions()
{
	for (FMoveObjectData& MoveData : MoveObjects)
	{
		if (IsValid(MoveData.TargetActor))
		{
			// 초기 위치와 회전 저장
			MoveData.InitialLocation = MoveData.TargetActor->GetActorLocation();
			MoveData.InitialRotation = MoveData.TargetActor->GetActorRotation();
			MoveData.bIsAtTarget = false;

			UE_LOG(LogCS, Log, TEXT("Initialized Actor %s - Location: %s, Rotation: %s"),
				*MoveData.TargetActor->GetName(),
				*MoveData.InitialLocation.ToString(),
				*MoveData.InitialRotation.ToString());
		}
	}
}

void ACSMoveObjectSwitch::CalculateTargetTransforms()
{
	for (FMoveObjectData& MoveData : MoveObjects)
	{
		if (IsValid(MoveData.TargetActor))
		{
			// 위치 계산
			if (MoveData.bUseActorLocalSpace)
			{
				// 액터의 로컬 좌표계 기준으로 오프셋 적용
				FTransform ActorTransform = MoveData.TargetActor->GetActorTransform();

				if (MoveData.bIgnoreScale)
				{
					// 스케일을 무시하고 순수한 회전만 적용
					FQuat ActorRotation = ActorTransform.GetRotation();
					FVector WorldOffset = ActorRotation.RotateVector(MoveData.LocalMovementOffset);
					MoveData.CalculatedTargetLocation = MoveData.InitialLocation + WorldOffset;
				}
				else
				{
					// 스케일 포함하여 변환
					FVector WorldOffset = ActorTransform.TransformVector(MoveData.LocalMovementOffset);
					MoveData.CalculatedTargetLocation = MoveData.InitialLocation + WorldOffset;
				}
			}
			else
			{
				// 월드 좌표계 기준으로 오프셋 적용 (스케일 무관)
				MoveData.CalculatedTargetLocation = MoveData.InitialLocation + MoveData.LocalMovementOffset;
			}

			// 회전 계산 (항상 상대적으로 적용)
			MoveData.CalculatedTargetRotation = MoveData.InitialRotation + MoveData.LocalRotationOffset;

			UE_LOG(LogCS, Log, TEXT("Calculated Target for %s:"),
				*MoveData.TargetActor->GetName());
			UE_LOG(LogCS, Log, TEXT("  - Actor Scale: %s"),
				*MoveData.TargetActor->GetActorScale3D().ToString());
			UE_LOG(LogCS, Log, TEXT("  - Movement Offset: %s (%s space, %s scale)"),
				*MoveData.LocalMovementOffset.ToString(),
				MoveData.bUseActorLocalSpace ? TEXT("Local") : TEXT("World"),
				MoveData.bIgnoreScale ? TEXT("Ignore") : TEXT("Apply"));
			UE_LOG(LogCS, Log, TEXT("  - Target Location: %s"),
				*MoveData.CalculatedTargetLocation.ToString());
			UE_LOG(LogCS, Log, TEXT("  - Rotation Offset: %s"),
				*MoveData.LocalRotationOffset.ToString());
			UE_LOG(LogCS, Log, TEXT("  - Target Rotation: %s"),
				*MoveData.CalculatedTargetRotation.ToString());
		}
	}
}

void ACSMoveObjectSwitch::StartMovement()
{
	if (bIsMoving)
	{
		UE_LOG(LogCS, Warning, TEXT("Movement already in progress, ignoring new request"));
		return;
	}

	// 이동할 유효한 오브젝트가 있는지 확인
	bool bHasValidTargets = false;
	for (const FMoveObjectData& MoveData : MoveObjects)
	{
		if (IsValid(MoveData.TargetActor))
		{
			bHasValidTargets = true;
			break;
		}
	}

	if (!bHasValidTargets)
	{
		UE_LOG(LogCS, Warning, TEXT("No valid target actors to move"));
		return;
	}

	// 이동 방향 결정 (스위치 상태에 따라)
	bMovingToTarget = bIsInteracted;
	bIsMoving = true;

	UE_LOG(LogCS, Log, TEXT("Starting movement - Direction: %s"),
		bMovingToTarget ? TEXT("To Target") : TEXT("To Initial"));

	// 클라이언트에 이동 시작 알림
	NetMulticastStartMovement(bMovingToTarget);
}

void ACSMoveObjectSwitch::UpdateMovement(float DeltaTime)
{
	bool bAllReachedDestination = true;
	TArray<FVector> NewLocations;
	TArray<FRotator> NewRotations;

	for (FMoveObjectData& MoveData : MoveObjects)
	{
		if (!IsValid(MoveData.TargetActor))
		{
			NewLocations.Add(FVector::ZeroVector);
			NewRotations.Add(FRotator::ZeroRotator);
			continue;
		}

		// === 위치 이동 처리 ===
		FVector CurrentLocation = MoveData.TargetActor->GetActorLocation();
		FVector TargetPosition = bMovingToTarget ? MoveData.CalculatedTargetLocation : MoveData.InitialLocation;
		FVector NewLocation = CurrentLocation;

		bool bLocationReached = true;
		if (MoveData.bEnableLocationMovement)
		{
			float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);
			if (DistanceToTarget > MoveTolerance)
			{
				// 일정한 속도로 위치 이동 (MoveSpeed 유닛/초)
				NewLocation = FMath::VInterpConstantTo(CurrentLocation, TargetPosition, DeltaTime, MoveSpeed);
				bLocationReached = false;
			}
			else
			{
				// 목표에 도달했으면 정확한 위치로 스냅
				NewLocation = TargetPosition;
			}
			MoveData.TargetActor->SetActorLocation(NewLocation);
		}

		// === 회전 처리 ===
		FRotator CurrentRotation = MoveData.TargetActor->GetActorRotation();
		FRotator TargetRotationValue = bMovingToTarget ? MoveData.CalculatedTargetRotation : MoveData.InitialRotation;
		FRotator NewRotation = CurrentRotation;

		bool bRotationReached = true;
		if (MoveData.bEnableRotationMovement)
		{
			// 회전 거리 계산 (각도 차이)
			FRotator RotationDifference = TargetRotationValue - CurrentRotation;
			RotationDifference.Normalize(); // -180~180 범위로 정규화

			float RotationDistance = FMath::Abs(RotationDifference.Pitch) +
				FMath::Abs(RotationDifference.Yaw) +
				FMath::Abs(RotationDifference.Roll);

			if (RotationDistance > RotationTolerance)
			{
				// 일정한 속도로 회전 (RotationSpeed 도/초)
				NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotationValue, DeltaTime, RotationSpeed);
				bRotationReached = false;
			}
			else
			{
				// 목표에 도달했으면 정확한 회전으로 스냅
				NewRotation = TargetRotationValue;
			}
			MoveData.TargetActor->SetActorRotation(NewRotation);
		}

		// 위치와 회전 모두 목표에 도달했는지 확인
		if (!bLocationReached || !bRotationReached)
		{
			bAllReachedDestination = false;
		}
		else
		{
			MoveData.bIsAtTarget = bMovingToTarget;
		}

		NewLocations.Add(NewLocation);
		NewRotations.Add(NewRotation);
	}

	// 클라이언트에 위치와 회전 정보 전송
	NetMulticastUpdateMovement(NewLocations, NewRotations);

	// 모든 오브젝트가 목표에 도달했으면 이동 완료
	if (bAllReachedDestination)
	{
		bIsMoving = false;
		UE_LOG(LogCS, Log, TEXT("Movement completed - All objects reached destination"));
	}
}

void ACSMoveObjectSwitch::NetMulticastStartMovement_Implementation(bool bInMoveToTarget)
{
	// 클라이언트에서는 이동 상태만 업데이트
	bMovingToTarget = bInMoveToTarget;
	bIsMoving = true;
}

void ACSMoveObjectSwitch::NetMulticastUpdateMovement_Implementation(const TArray<FVector>& NewLocations, const TArray<FRotator>& NewRotations)
{
	// 클라이언트에서 위치와 회전 업데이트
	if (!HasAuthority())
	{
		for (int32 i = 0; i < MoveObjects.Num() && i < NewLocations.Num() && i < NewRotations.Num(); i++)
		{
			if (IsValid(MoveObjects[i].TargetActor))
			{
				// 위치 업데이트
				if (MoveObjects[i].bEnableLocationMovement)
				{
					MoveObjects[i].TargetActor->SetActorLocation(NewLocations[i]);
				}

				// 회전 업데이트
				if (MoveObjects[i].bEnableRotationMovement)
				{
					MoveObjects[i].TargetActor->SetActorRotation(NewRotations[i]);
				}
			}
		}
	}
}