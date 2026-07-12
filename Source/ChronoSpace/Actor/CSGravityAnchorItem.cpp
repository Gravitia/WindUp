// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSGravityAnchorItem.h"
#include "Components/StaticMeshComponent.h"

ACSGravityAnchorItem::ACSGravityAnchorItem()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticMeshRef(TEXT("/Script/Engine.StaticMesh'/Game/30_Mesh/StaticMesh/SM_Cube.SM_Cube'"));
	if (StaticMeshRef.Object)
	{
		Mesh->SetStaticMesh(StaticMeshRef.Object);
	}

	// 플레이어가 밀어서 옮길 수 있는 물리 오브젝트 (서버 물리 -> 클라이언트 복제)
	Mesh->SetRelativeScale3D(FVector(0.5f));
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
}
