// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSReverseGravityField.h"
#include "Actor/CSGravityAnchorItem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"

ACSReverseGravityField::ACSReverseGravityField()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	FieldBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FieldBox"));
	FieldBox->SetBoxExtent(FVector(50.0f));
	FieldBox->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	FieldBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(FieldBox);

	// 중력 반전과 동일한 방식으로 양쪽(서버/클라)에서 로컬 오버랩 처리
	FieldBox->OnComponentBeginOverlap.AddDynamic(this, &ACSReverseGravityField::OnFieldBeginOverlap);
	FieldBox->OnComponentEndOverlap.AddDynamic(this, &ACSReverseGravityField::OnFieldEndOverlap);

	FieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FieldMesh"));
	FieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldMesh->SetVisibility(false);
	FieldMesh->SetupAttachment(FieldBox);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticMeshRef(TEXT("/Script/Engine.StaticMesh'/Game/30_Mesh/StaticMesh/SM_Cube.SM_Cube'"));
	if (StaticMeshRef.Object)
	{
		FieldMesh->SetStaticMesh(StaticMeshRef.Object);
	}

	// 중력 반전 머티리얼 (파란색) 재사용
	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialRef(TEXT("/Script/Engine.Material'/Game/31_Material/MAT_AntyGravity.MAT_AntyGravity'"));
	if (MaterialRef.Object)
	{
		FieldMaterial = MaterialRef.Object;
	}
}

void ACSReverseGravityField::BeginPlay()
{
	Super::BeginPlay();

	if (FieldMaterial)
	{
		FieldMID = UMaterialInstanceDynamic::Create(FieldMaterial, this);
		FieldMesh->SetMaterial(0, FieldMID);
	}

	// 영역 계산은 서버에서만, 결과는 리플리케이션으로 전달
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(UpdateTimerHandle, this, &ACSReverseGravityField::UpdateField, UpdateInterval, true);
	}
}

void ACSReverseGravityField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSReverseGravityField, FieldCenter);
	DOREPLIFETIME(ACSReverseGravityField, FieldExtent);
	DOREPLIFETIME(ACSReverseGravityField, bFieldActive);
}

void ACSReverseGravityField::UpdateField()
{
	// 같은 채널의 앵커 아이템 수집
	TArray<ACSGravityAnchorItem*> Items;
	for (TActorIterator<ACSGravityAnchorItem> It(GetWorld()); It; ++It)
	{
		if (It->GetGroupChannel() == GroupChannel)
		{
			Items.Add(*It);
		}
	}

	if (Items.Num() < RequiredItemCount)
	{
		if (bFieldActive)
		{
			bFieldActive = false;
			ApplyFieldState();
		}
		return;
	}

	// 아이템들의 XY 범위 계산 (Z는 최저점 기준 위로 FieldHeight)
	FVector Min = Items[0]->GetActorLocation();
	FVector Max = Min;
	for (ACSGravityAnchorItem* Item : Items)
	{
		Min = Min.ComponentMin(Item->GetActorLocation());
		Max = Max.ComponentMax(Item->GetActorLocation());
	}

	FieldCenter = FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Min.Z + FieldHeight * 0.5f);
	FieldExtent = FVector(
		FMath::Max((Max.X - Min.X) * 0.5f, 50.0f),
		FMath::Max((Max.Y - Min.Y) * 0.5f, 50.0f),
		FieldHeight * 0.5f);
	bFieldActive = true;

	ApplyFieldState();
}

void ACSReverseGravityField::OnRep_FieldState()
{
	ApplyFieldState();
}

void ACSReverseGravityField::ApplyFieldState()
{
	SetActorLocation(FieldCenter);
	FieldBox->SetBoxExtent(FieldExtent);
	FieldBox->SetCollisionEnabled(bFieldActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	// SM_Cube 기본 크기 100x100x100 (피벗 코너) -> 영역 크기에 맞춰 스케일/센터 보정
	const float HalfSizeOfSide = 50.0f;
	const FVector MeshScale = FieldExtent / HalfSizeOfSide;
	FieldMesh->SetRelativeScale3D(MeshScale);
	FieldMesh->SetRelativeLocation(FVector(-HalfSizeOfSide) * MeshScale);
	FieldMesh->SetVisibility(bFieldActive);

	if (FieldMID)
	{
		float OutBaseValue = 2.0f;
		FieldMID->GetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue);
		FieldMID->SetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue * MeshScale.X);
	}
}

void ACSReverseGravityField::OnFieldBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// CSTA_ReverseGravityBox와 동일한 방식
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);
	if (DetectedCharacter == nullptr) return;

	UCharacterMovementComponent* MovementComp = DetectedCharacter->GetCharacterMovement();
	if (MovementComp == nullptr || MovementComp->GravityScale < 0) return;

	MovementComp->AddImpulse(FVector(0.0f, 0.0f, 0.1f));
	MovementComp->GravityScale *= -1.0f;
}

void ACSReverseGravityField::OnFieldEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* DetectedCharacter = Cast<ACharacter>(OtherActor);
	if (DetectedCharacter == nullptr) return;

	UCharacterMovementComponent* MovementComp = DetectedCharacter->GetCharacterMovement();
	if (MovementComp == nullptr || MovementComp->GravityScale > 0) return;

	MovementComp->AddImpulse(FVector(0.0f, 0.0f, 0.1f));
	MovementComp->GravityScale *= -1.0f;
}
