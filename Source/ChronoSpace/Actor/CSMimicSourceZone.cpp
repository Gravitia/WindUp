// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMimicSourceZone.h"
#include "Actor/CSMimicTargetZone.h"
#include "Character/CSCharacterPlayer.h"
#include "Character/CSMimicCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Subsystem/CSMimicWorldSubsystem.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "Physics/CSCollision.h"
#include "ChronoSpace.h"

ACSMimicSourceZone::ACSMimicSourceZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	ZoneBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	ZoneBox->SetCollisionProfileName(CPROFILE_OVERLAPALL);
	SetRootComponent(ZoneBox);

	// 구역 표시 메시 (중력반전 박스와 동일한 패턴)
	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMesh->SetupAttachment(ZoneBox);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticMeshRef(TEXT("/Script/Engine.StaticMesh'/Game/30_Mesh/StaticMesh/SM_Cube.SM_Cube'"));
	if (StaticMeshRef.Object)
	{
		ZoneMesh->SetStaticMesh(StaticMeshRef.Object);
	}

	// 파란색 - 중력반전 머티리얼 재사용
	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialRef(TEXT("/Script/Engine.Material'/Game/31_Material/MAT_AntyGravity.MAT_AntyGravity'"));
	if (MaterialRef.Object)
	{
		ZoneMaterial = MaterialRef.Object;
	}
}

void ACSMimicSourceZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateZoneMesh();
}

void ACSMimicSourceZone::UpdateZoneMesh()
{
	if (ZoneMesh == nullptr || ZoneBox == nullptr) return;

	// SM_Cube 기본 크기 100x100x100 (피벗 코너) -> 박스 크기에 맞춰 스케일/센터 보정
	const float HalfSizeOfSide = 50.0f;
	const FVector MeshScale = ZoneBox->GetUnscaledBoxExtent() / HalfSizeOfSide;

	ZoneMesh->SetRelativeScale3D(MeshScale);
	ZoneMesh->SetRelativeLocation(FVector(-HalfSizeOfSide) * MeshScale);

	if (ZoneMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
		if (DynamicMaterial)
		{
			float OutBaseValue = 2.0f;
			DynamicMaterial->GetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue);
			DynamicMaterial->SetScalarParameterValue(FName(TEXT("Tiling")), OutBaseValue * MeshScale.X);
			ZoneMesh->SetMaterial(0, DynamicMaterial);
		}
	}
}

void ACSMimicSourceZone::BeginPlay()
{
	Super::BeginPlay();

	// 분신 스폰 판정은 서버에서만 한다
	if (HasAuthority())
	{
		ZoneBox->OnComponentBeginOverlap.AddDynamic(this, &ACSMimicSourceZone::OnZoneBeginOverlap);
		ZoneBox->OnComponentEndOverlap.AddDynamic(this, &ACSMimicSourceZone::OnZoneEndOverlap);
	}
}

void ACSMimicSourceZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Release();

	Super::EndPlay(EndPlayReason);
}

void ACSMimicSourceZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 이미 점유 중이면 무시 (한 구역에 한 명)
	if (Occupant.IsValid()) return;

	// 분신은 파란 구역을 점유할 수 없다
	if (Cast<ACSMimicCharacter>(OtherActor)) return;

	ACSCharacterPlayer* Player = Cast<ACSCharacterPlayer>(OtherActor);
	if (Player == nullptr || !Player->IsPlayerControlled()) return;

	Occupy(Player);
}

void ACSMimicSourceZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 점유자가 나가면 분신 소멸
	if (Occupant.IsValid() && OtherActor == Occupant.Get())
	{
		Release();
	}
}

void ACSMimicSourceZone::Occupy(ACSCharacterPlayer* Player)
{
	if (MimicCharacterClass == nullptr)
	{
		UE_LOG(LogCS, Warning, TEXT("ACSMimicSourceZone(%s) : MimicCharacterClass is not set"), *GetName());
		return;
	}

	UCSMimicWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UCSMimicWorldSubsystem>();
	if (Subsystem == nullptr) return;

	TArray<ACSMimicTargetZone*> TargetZones = Subsystem->GetTargetZones(LinkChannel);
	if (TargetZones.Num() == 0)
	{
		UE_LOG(LogCS, Warning, TEXT("ACSMimicSourceZone(%s) : No target zone for channel '%s'"), *GetName(), *LinkChannel.ToString());
		return;
	}

	Occupant = Player;

	const FTransform SourceZoneTM = GetActorTransform();

	for (ACSMimicTargetZone* TargetZone : TargetZones)
	{
		const FTransform TargetZoneTM = TargetZone->GetActorTransform();

		// 파란 구역 기준 상대 위치/방향을 빨간 구역 기준으로 매핑해 스폰
		const FVector RelativeLoc = SourceZoneTM.InverseTransformPositionNoScale(Player->GetActorLocation());
		const FVector SpawnLoc = TargetZoneTM.TransformPositionNoScale(RelativeLoc);
		const FQuat ZoneDeltaQuat = TargetZoneTM.GetRotation() * SourceZoneTM.GetRotation().Inverse();
		const FQuat SpawnQuat = ZoneDeltaQuat * Player->GetActorQuat();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ACSMimicCharacter* Mimic = GetWorld()->SpawnActor<ACSMimicCharacter>(MimicCharacterClass, SpawnLoc, SpawnQuat.Rotator(), Params);
		if (Mimic)
		{
			Mimic->InitMimic(Player, SourceZoneTM, TargetZoneTM);
			SpawnedMimics.Add(Mimic);
		}
	}

	// 능력 입력 미러링 구독
	if (UCSGASManagerComponent* GASManager = Player->GetGASManagerComponent())
	{
		GASManager->OnServerGASInput.AddUObject(this, &ACSMimicSourceZone::OnSourceGASInput);
	}
}

void ACSMimicSourceZone::Release()
{
	if (Occupant.IsValid())
	{
		if (UCSGASManagerComponent* GASManager = Occupant->GetGASManagerComponent())
		{
			GASManager->OnServerGASInput.RemoveAll(this);
		}
	}
	Occupant.Reset();

	for (ACSMimicCharacter* Mimic : SpawnedMimics)
	{
		if (IsValid(Mimic))
		{
			Mimic->Destroy();
		}
	}
	SpawnedMimics.Empty();
}

void ACSMimicSourceZone::OnSourceGASInput(int32 InputId, bool bPressed)
{
	for (ACSMimicCharacter* Mimic : SpawnedMimics)
	{
		if (IsValid(Mimic))
		{
			Mimic->MirrorAbilityInput(InputId, bPressed);
		}
	}
}
