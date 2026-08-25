// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSSwitchBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Physics/CSCollision.h"
#include "DataAsset/CSSwitchBaseData.h"
#include "Net/UnrealNetwork.h"
#include "ChronoSpace.h"

// Sets default values
ACSSwitchBase::ACSSwitchBase()
{
	bReplicates = true;
	bIsInteracted = false;

	// Static Mesh Comp
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	StaticMeshComp->SetIsReplicated(true);
	RootComponent = StaticMeshComp;
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticMeshRef(TEXT("/Script/Engine.StaticMesh'/Game/30_Mesh/Switch/functional_elements.functional_elements'"));
	if ( StaticMeshRef.Succeeded() )
	{
		StaticMeshComp->SetStaticMesh(StaticMeshRef.Object);
	}

	// Trigger
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetSphereRadius(80.0f, true);
	Trigger->SetupAttachment(StaticMeshComp);
	Trigger->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	Trigger->SetCollisionProfileName(CPROFILE_OVERLAPALL);

	// Widget
	InteractionPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptComponent"));
	InteractionPromptComponent->SetupAttachment(Trigger);
	InteractionPromptComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));

	static ConstructorHelpers::FClassFinder<UUserWidget> InteractionPromptWidgetRef(TEXT("/Game/01_Blueprint/UI/BP_InteractionPrompt.BP_InteractionPrompt_C"));
	if (InteractionPromptWidgetRef.Class)
	{
		InteractionPromptComponent->SetWidgetClass(InteractionPromptWidgetRef.Class);
		InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
		InteractionPromptComponent->SetDrawSize(FVector2D(500.0f, 30.f));
		InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	InteractionPromptComponent->SetVisibility(false);
}

void ACSSwitchBase::BeginInteraction()
{
	// Data 는 EditAnywhere 라 비어 있을 수 있다 - 비어 있으면 생성자 기본 위젯(BP_InteractionPrompt)을 그대로 쓴다.
	// (예전 조건은 뒤집혀 있어서 "클래스가 아직 로드되지 않았을 때만" 적용했다 - 다른 스위치가 먼저 로드해 두면
	//  이 스위치는 Data 의 위젯을 적용하지 않았다. 지금은 Data 에 위젯이 지정돼 있으면 항상 적용한다.)
	if (Data && !Data->InteractionPromptWidgetClass.IsNull())
	{
		UClass* WidgetClass = Data->InteractionPromptWidgetClass.LoadSynchronous();
		if (WidgetClass && InteractionPromptComponent->GetWidgetClass() != WidgetClass)
		{
			InteractionPromptComponent->SetWidgetClass(WidgetClass);
			InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
			InteractionPromptComponent->SetDrawSize(FVector2D(500.0f, 30.f));
			InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	InteractionPromptComponent->SetVisibility(true);
}

void ACSSwitchBase::EndInteraction()
{
	InteractionPromptComponent->SetVisibility(false);
}

void ACSSwitchBase::Interact()
{
	// 상태 변경은 서버 권한. (호출처인 UCSPlayerInteractionComponent 는 서버에서만 부르지만
	//  BP 에서 직접 부르면 클라에서만 토글되어 서버와 어긋난다)
	if (!HasAuthority()) return;

	UE_LOG(LogCS, Log, TEXT("[Netmode : %d] Interact"), GetWorld()->GetNetMode());
	bIsInteracted = !bIsInteracted;

	SetMaterial();
}

void ACSSwitchBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSSwitchBase, bIsInteracted);
}

void ACSSwitchBase::OnRep_IsInteracted()
{
	ApplyMaterial();
}

void ACSSwitchBase::BeginPlay()
{
	Super::BeginPlay();

	SetMaterial();
}

void ACSSwitchBase::SetMaterial()
{
	// 값은 호출 전에 이미 바뀌어 있다. 복제는 엔진이 클라로 보내고, 호스트는 여기서 직접 적용한다.
	ApplyMaterial();
}

void ACSSwitchBase::ApplyMaterial()
{
	if (Data == nullptr) return;

	//UE_LOG(LogCS, Log, TEXT("[NetMode : %d] NetMulticastSetMaterial_Implementation, %d"), GetWorld()->GetNetMode(), bIsInteracted);
	if (bIsInteracted)
	{
		if ( Data->MaterialSolidInteracted.IsValid() )
		{
			StaticMeshComp->SetMaterial(1, Data->MaterialSolidInteracted.Get());
		}
		else
		{
			Data->MaterialSolidInteracted.LoadSynchronous();
			StaticMeshComp->SetMaterial(1, Data->MaterialSolidInteracted.Get()); 
		}

		if ( Data->MaterialGlowInteracted.IsValid() )
		{
			StaticMeshComp->SetMaterial(3, Data->MaterialGlowInteracted.Get());  
		} 
		else
		{
			Data->MaterialGlowInteracted.LoadSynchronous();
			StaticMeshComp->SetMaterial(3, Data->MaterialGlowInteracted.Get());
		}
	}
	else
	{
		if (Data->MaterialSolidNonInteracted.IsValid())
		{
			StaticMeshComp->SetMaterial(1, Data->MaterialSolidNonInteracted.Get());
		}
		else
		{
			Data->MaterialSolidNonInteracted.LoadSynchronous();
			StaticMeshComp->SetMaterial(1, Data->MaterialSolidNonInteracted.Get());
		}

		if (Data->MaterialGlowNonInteracted.IsValid())
		{
			StaticMeshComp->SetMaterial(3, Data->MaterialGlowNonInteracted.Get());
		}
		else
		{
			Data->MaterialGlowNonInteracted.LoadSynchronous();
			StaticMeshComp->SetMaterial(3, Data->MaterialGlowNonInteracted.Get());
		}
	}
}



