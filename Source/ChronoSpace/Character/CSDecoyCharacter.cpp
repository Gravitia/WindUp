// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CSDecoyCharacter.h"
#include "Components/SphereComponent.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"

ACSDecoyCharacter::ACSDecoyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	WindUpTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("WindUpTrigger"));
	WindUpTrigger->SetSphereRadius(WindUpRange);
	WindUpTrigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	WindUpTrigger->SetupAttachment(RootComponent);
}

void ACSDecoyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (WindUpTrigger)
	{
		WindUpTrigger->SetSphereRadius(WindUpRange);
	}
}

void ACSDecoyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 태엽 감기 판정은 서버에서만 한다
	if (HasAuthority())
	{
		WindUpTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACSDecoyCharacter::OnWindUpTriggerBeginOverlap);
		WindUpTrigger->OnComponentEndOverlap.AddDynamic(this, &ACSDecoyCharacter::OnWindUpTriggerEndOverlap);
	}
}

void ACSDecoyCharacter::OnWindUpTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (Player == nullptr || !Player->IsPlayerControlled()) return;

	NearbyPlayers.Add(Player);
}

void ACSDecoyCharacter::OnWindUpTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor))
	{
		NearbyPlayers.Remove(Player);
	}
}

void ACSDecoyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority()) return;

	// 걷는 중 - 시간이 다 되면 멈춘다
	if (bWalking)
	{
		WalkElapsed += DeltaSeconds;
		if (WalkElapsed >= WalkDuration)
		{
			StopWalking();
		}
		else
		{
			AddMovementInput(WalkDirection, 1.0f);
		}
		return;
	}

	// 대기 상태 - 플레이어가 근처에 머무는 시간을 누적
	bool bPlayerNearby = false;
	for (auto It = NearbyPlayers.CreateIterator(); It; ++It)
	{
		if (It->IsValid())
		{
			bPlayerNearby = true;
		}
		else
		{
			It.RemoveCurrent();
		}
	}

	if (bPlayerNearby)
	{
		WindUpElapsed += DeltaSeconds;
		OnWindUpProgress(FMath::Clamp(WindUpElapsed / WindUpRequiredTime, 0.0f, 1.0f));

		if (WindUpElapsed >= WindUpRequiredTime)
		{
			StartWalking();
		}
	}
	else if (WindUpElapsed > 0.0f)
	{
		// 범위를 벗어나면 감던 태엽은 풀린다
		WindUpElapsed = 0.0f;
		OnWindUpProgress(0.0f);
	}
}

void ACSDecoyCharacter::StartWalking()
{
	WindUpElapsed = 0.0f;
	WalkElapsed = 0.0f;
	bWalking = true;
	WalkDirection = GetActorForwardVector();

	OnWindUpComplete();

	UE_LOG(LogCS, Log, TEXT("ACSDecoyCharacter(%s) : WindUp complete, walking for %.1fs"), *GetName(), WalkDuration);
}

void ACSDecoyCharacter::StopWalking()
{
	bWalking = false;
	WalkElapsed = 0.0f;

	OnWalkFinished();

	UE_LOG(LogCS, Log, TEXT("ACSDecoyCharacter(%s) : Walk finished"), *GetName());
}
