// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/GridPuzzle/CSGridLever.h"
#include "Actor/GridPuzzle/CSGridBoard.h"
#include "Actor/GridPuzzle/CSGridBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/UserWidget.h"
#include "Physics/CSCollision.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

ACSGridLever::ACSGridLever()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 레버 기둥 메시 (엔진 기본 실린더)
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 1.0f));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshRef(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshRef.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMeshRef.Object);
	}

	// Cylinder 애셋의 기본 머티리얼(WorldGridMaterial)에는 Color 파라미터가 없어서
	// 색 변경이 되도록 BasicShapeMaterial 로 교체한다.
	static ConstructorHelpers::FObjectFinder<UMaterial> BaseMaterialRef(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterialRef.Succeeded())
	{
		Mesh->SetMaterial(0, BaseMaterialRef.Object);
	}

	// 상호작용 감지 트리거 (플레이어 InteractionComponent 가 이 오버랩으로 대상을 잡는다)
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetSphereRadius(150.0f, true);
	Trigger->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Trigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);

	// 상호작용 프롬프트 위젯 (CSSwitchBase 와 동일한 위젯 사용)
	InteractionPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptComponent"));
	InteractionPromptComponent->SetupAttachment(Root);
	InteractionPromptComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));

	static ConstructorHelpers::FClassFinder<UUserWidget> InteractionPromptWidgetRef(TEXT("/Game/01_Blueprint/UI/BP_InteractionPrompt.BP_InteractionPrompt_C"));
	if (InteractionPromptWidgetRef.Class)
	{
		InteractionPromptComponent->SetWidgetClass(InteractionPromptWidgetRef.Class);
		InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
		InteractionPromptComponent->SetDrawSize(FVector2D(500.0f, 30.0f));
		InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	InteractionPromptComponent->SetVisibility(false);
}

void ACSGridLever::BeginPlay()
{
	Super::BeginPlay();

	LeverMID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	ApplyLeverColor();
}

void ACSGridLever::BeginInteraction()
{
	InteractionPromptComponent->SetVisibility(true);
}

void ACSGridLever::EndInteraction()
{
	InteractionPromptComponent->SetVisibility(false);
}

void ACSGridLever::Interact()
{
	// UCSPlayerInteractionComponent 흐름상 서버에서 호출되지만 한 번 더 확인
	if (!HasAuthority())
	{
		return;
	}

	if (bUsed && bSingleUse)
	{
		return;
	}

	SpawnBox();
}

void ACSGridLever::SpawnBox()
{
	FVector BaseLocation;
	FRotator SpawnRotation = GetActorRotation();

	if (Board && Board->IsValidCell(SpawnRow, SpawnCol))
	{
		BaseLocation = Board->CellToWorld(SpawnRow, SpawnCol);
		BaseLocation.Z = Board->GetTileTopZ(SpawnRow, SpawnCol);
		SpawnRotation = Board->GetActorRotation();
	}
	else
	{
		BaseLocation = GetActorLocation() + GetActorForwardVector() * 250.0f;
	}

	// 스폰 칸이 박스/캐릭터로 막혀 있으면 스폰하지 않는다.
	const FVector TestCenter = BaseLocation + FVector(0.0f, 0.0f, SpawnCheckExtent.Z + 5.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CSGridLeverSpawn), false, this);
	if (GetWorld()->OverlapBlockingTestByChannel(TestCenter, SpawnRotation.Quaternion(), ECC_WorldDynamic, FCollisionShape::MakeBox(SpawnCheckExtent), Params))
	{
		UE_LOG(LogCS, Log, TEXT("[GridLever] %s : spawn cell is blocked"), *GetName());
		return;
	}

	UClass* ClassToSpawn = BoxClass ? *BoxClass : ACSGridBox::StaticClass();
	const FTransform SpawnTransform(SpawnRotation, BaseLocation);

	ACSGridBox* NewBox = GetWorld()->SpawnActorDeferred<ACSGridBox>(ClassToSpawn, SpawnTransform, this);
	if (NewBox == nullptr)
	{
		return;
	}

	NewBox->SetBoard(Board);
	NewBox->FinishSpawning(SpawnTransform);

	// 박스 바닥이 타일 윗면에 오도록 높이 보정
	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	NewBox->GetActorBounds(true, Origin, Extent);

	FVector BoxLocation = NewBox->GetActorLocation();
	BoxLocation.Z = BaseLocation.Z + Extent.Z + 1.0f;
	NewBox->SetActorLocation(BoxLocation);

	UE_LOG(LogCS, Log, TEXT("[GridLever] %s : spawned %s"), *GetName(), *NewBox->GetName());

	bUsed = true;
	OnRep_Used();	// 리슨 호스트는 OnRep 을 받지 않는다
}

void ACSGridLever::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSGridLever, bUsed);
}

void ACSGridLever::OnRep_Used()
{
	ApplyLeverColor();
}

void ACSGridLever::ApplyLeverColor()
{
	if (LeverMID == nullptr && Mesh)
	{
		LeverMID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (LeverMID)
	{
		LeverMID->SetVectorParameterValue(TEXT("Color"), bUsed ? UsedColor : IdleColor);
	}
}
