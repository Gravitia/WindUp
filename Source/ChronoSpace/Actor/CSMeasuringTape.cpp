// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMeasuringTape.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

ACSMeasuringTape::ACSMeasuringTape()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // Root
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainMesh"));
    RootComponent = Mesh;

    // Eyes
    EyesMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyesMesh"));
    EyesMesh->SetupAttachment(RootComponent);

    // Nose
    NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
    NoseMesh->SetupAttachment(RootComponent);

    // Button
    ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
    ButtonMesh->SetupAttachment(RootComponent);

    // Default Ruler
    RulerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RulerMesh"));
    RulerMesh->SetupAttachment(RootComponent);

    // Trigger Box
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    Trigger->SetupAttachment(RootComponent);
    Trigger->SetBoxExtent(FVector(64.f, 64.f, 64.f));
    Trigger->SetCollisionProfileName("Trigger");
}

// Called when the game starts or when spawned
void ACSMeasuringTape::BeginPlay()
{
	Super::BeginPlay();

    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACSMeasuringTape::OnTriggerBegin);
    Trigger->OnComponentEndOverlap.AddDynamic(this, &ACSMeasuringTape::OnTriggerEnd);
}

void ACSMeasuringTape::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 부드럽게 보간
    CurrentScaleInternal = FMath::FInterpTo(CurrentScaleInternal, TargetScale, DeltaTime, LerpSpeed);

    FVector NewScale = FVector(CurrentScaleInternal, 1.0f, 1.0f);
    RulerMesh->SetRelativeScale3D(NewScale);
}

void ACSMeasuringTape::OnTriggerBegin(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    // 플레이어인지 체크
    if (OtherActor && OtherActor != this)
    {
        SetRulerScale(TargetRulerScale);  // 5배
    }
}

void ACSMeasuringTape::OnTriggerEnd(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;

    SetRulerScale(1.0f);  // 원래 크기
}

void ACSMeasuringTape::SetRulerScale(float NewScale)
{
    TargetScale = NewScale;
    OnRep_TargetScale();   // 서버에서도 즉시 반영됨
}

void ACSMeasuringTape::OnRep_TargetScale()
{
    // 리플리케이션은 목표값만 업데이트
    // 실제 보간된 스케일은 Tick에서 처리
}

void ACSMeasuringTape::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSMeasuringTape, TargetScale);
}