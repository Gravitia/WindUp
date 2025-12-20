// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSMeasuringTape.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/AudioComponent.h"

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


    // Sound
    RulerAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("RulerAudio"));
    RulerAudioComponent->SetupAttachment(RootComponent);
    RulerAudioComponent->bAutoActivate = false;
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

    CurrentScaleInternal = FMath::FInterpTo(
        CurrentScaleInternal,
        TargetScale,
        DeltaTime,
        LerpSpeed
    );

    RulerMesh->SetRelativeScale3D(
        FVector(CurrentScaleInternal, 1.f, 1.f)
    );

    // 목표 도달 시 사운드 종료
    if (FMath::IsNearlyEqual(CurrentScaleInternal, TargetScale, 0.01f))
    {
        if (RulerAudioComponent && RulerAudioComponent->IsPlaying())
        {
            RulerAudioComponent->Stop();
        }

        bIsExtending = false;
        bIsRetracting = false;
    }
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
    if (TargetScale == NewScale)
        return;

    const bool bExtend = NewScale > TargetScale;
    const bool bRetract = NewScale < TargetScale;

    TargetScale = NewScale;
    OnRep_TargetScale();

    //  사운드 제어
    if (bExtend && ExtendSound)
    {
        RulerAudioComponent->Stop();
        RulerAudioComponent->SetSound(ExtendSound);
        RulerAudioComponent->Play();
        bIsExtending = true;
        bIsRetracting = false;
    }
    else if (bRetract && RetractSound)
    {
        RulerAudioComponent->Stop();
        RulerAudioComponent->SetSound(RetractSound);
        RulerAudioComponent->Play();
        bIsRetracting = true;
        bIsExtending = false;
    }
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