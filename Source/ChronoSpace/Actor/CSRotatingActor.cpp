// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSRotatingActor.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

// Sets default values
ACSRotatingActor::ACSRotatingActor()
{
	bReplicates = true;
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Static Mesh 컴포넌트 생성 및 루트 컴포넌트로 설정
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

}

// Called when the game starts or when spawned
void ACSRotatingActor::BeginPlay()
{
	Super::BeginPlay();

	InitialRotation = GetActorRotation();
}

float ACSRotatingActor::GetSynchronizedWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.0f;
}

// Called every frame
void ACSRotatingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 각 머신이 로컬 DeltaTime 을 누적하면 프레임레이트 차이와 hitch 로 서버/클라 각도가 계속 벌어졌다
	// (회전 플랫폼 위 클라 캐릭터가 서버 기준으로는 밖에 있어 밀려 떨어짐).
	// 서버 시각으로 절대 각도를 계산하면 복제 없이도 모든 머신이 일치한다 - CSAnimatedTrap 과 같은 방식.
	const float Yaw = RotationSpeed * GetSynchronizedWorldTime();
	SetActorRotation(InitialRotation + FRotator(0.0f, Yaw, 0.0f));
}

