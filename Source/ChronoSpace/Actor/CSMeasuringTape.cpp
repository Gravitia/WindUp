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

    // 기본 트랜스폼 저장
    EyesBaseRotation = EyesMesh->GetRelativeRotation();

    NoseBaseRotation = NoseMesh->GetRelativeRotation();

    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACSMeasuringTape::OnTriggerBegin);
    Trigger->OnComponentEndOverlap.AddDynamic(this, &ACSMeasuringTape::OnTriggerEnd);
}

void ACSMeasuringTape::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 기존 줄자 로직 그대로
    CurrentScaleInternal = FMath::FInterpTo(
        CurrentScaleInternal,
        TargetScale,
        DeltaTime,
        LerpSpeed
    );

    RulerMesh->SetRelativeScale3D(FVector(CurrentScaleInternal, 1.f, 1.f));

    // 얼굴 반응 보간
    if (bFaceReacting)
    {

        EyesMesh->SetRelativeRotation(
            FMath::RInterpTo(
                EyesMesh->GetRelativeRotation(),
                EyesTargetRotation,
                DeltaTime,
                FaceLerpSpeed
            )
        );

        NoseMesh->SetRelativeRotation(
            FMath::RInterpTo(
                NoseMesh->GetRelativeRotation(),
                NoseTargetRotation,
                DeltaTime,
                FaceLerpSpeed
            )
        );

        if (
            EyesMesh->GetRelativeRotation().Equals(EyesTargetRotation, 0.5f) &&
            NoseMesh->GetRelativeRotation().Equals(NoseTargetRotation, 0.5f)
            )
        {
            bFaceReacting = false;
        }
    }
}


void ACSMeasuringTape::OnTriggerBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;
    if (!OtherActor || OtherActor == this) return;

    SetRulerScale(TargetRulerScale);

    // 목표값 계산만 수행
    EyesTargetRotation = EyesBaseRotation + EyesReactRotation;
    NoseTargetRotation = NoseBaseRotation + NoseReactRotation;

    bFaceReacting = true;
}

void ACSMeasuringTape::OnTriggerEnd(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{

    SetRulerScale(1.0f);

    // 무조건 베이스로 복귀
    EyesTargetRotation = EyesBaseRotation;
    NoseTargetRotation = NoseBaseRotation;

    bFaceReacting = true;
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