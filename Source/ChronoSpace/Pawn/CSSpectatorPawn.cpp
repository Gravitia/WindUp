// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/CSSpectatorPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ChronoSpace.h"
#include "Physics/CSCollision.h"
#include "DataAsset/CSCharacterPlayerData.h"

FVector SplitScreen::CalculateOffsetPosition(const FVector& TargetPos, const FRotator& TargetRot)
{
	FVector RotatedOffset = TargetRot.RotateVector(CameraOffset);
	return TargetPos + RotatedOffset;
}

FRotator SplitScreen::CalculateLookAtRotation(const FVector& FromPos, const FVector& ToPos)
{
	FVector Direction = (ToPos - FromPos).GetSafeNormal();
	return FRotationMatrix::MakeFromX(Direction).Rotator();
}

UCameraComponent* SplitScreen::FindCameraInPawn(APawn* Pawn)
{
    if (!Pawn) return nullptr;

    // 1. 직접 CameraComponent 찾기
    UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (Camera)
    {
        UE_LOG(LogTemp, Log, TEXT("SS Dummy: Found camera directly: %s"), *Camera->GetName());
        return Camera;
    }

    // 2. 재귀적으로 모든 자식 컴포넌트 검사
    TArray<UCameraComponent*> CameraComponents;
    Pawn->GetComponents<UCameraComponent>(CameraComponents);

    if (CameraComponents.Num() > 0)
    {
        UE_LOG(LogCS, Log, TEXT("SS Dummy: Found camera recursively: %s"), *CameraComponents[0]->GetName());
        return CameraComponents[0];
    }

    UE_LOG(LogCS, Warning, TEXT("SS Dummy: No camera found in pawn: %s"), *Pawn->GetName());
    return nullptr;
}

ACSSpectatorPawn::ACSSpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1) SkeletalMesh 먼저 생성 → RootComponent 지정
    SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    RootComponent = SkeletalMesh;
    SkeletalMesh->SetVisibility(false);   // 보이지 않게
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 2) SpringArm을 SkeletalMesh에 붙임
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(SkeletalMesh);

    CameraBoom->TargetArmLength = 0.f;
    static ConstructorHelpers::FObjectFinder<UCSCharacterPlayerData> PlayerDataRef(TEXT("/Game/04_DataAssets/Character/BPDA_CharacterPlayerData.BPDA_CharacterPlayerData"));
    if ( PlayerDataRef.Succeeded() )
    {
        UCSCharacterPlayerData* PlayerData = PlayerDataRef.Object;
        CameraBoom->TargetArmLength = PlayerData->TargetArmLength;
        CameraBoom->SetRelativeLocation( PlayerData->CameraOffset );
        UE_LOG(LogCS, Log, TEXT("ACSSpectatorPawn - Success"));
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("ACSSpectatorPawn - No Data"));
    }

    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->ProbeChannel = CCHANNEL_CSSPECTATOR;  // 별도 카메라로 수정 필요

    // 3) 카메라 생성해서 SpringArm에 붙임
    DummyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DummyCamera"));
    DummyCamera->SetupAttachment(CameraBoom);
    DummyCamera->bUsePawnControlRotation = false;

    // 랙 제거
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 40.f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 60.f;

    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    // Pawn 안보이게
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void ACSSpectatorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    //  타겟이 사라졌는지 체크하고 자동으로 재탐색
    if (!TargetCamera && TargetPawn)
    {
        UpdateTargetCamera();
    }
}

void ACSSpectatorPawn::SyncWithRemotePlayer(FVector Location, FRotator Rotation)
{
    TargetLocation = Location;
    TargetRotation = Rotation;
    bHasTarget = true;
}

void ACSSpectatorPawn::SyncWithRemoteCamera(UCameraComponent* RemoteCamera)
{
    if (!RemoteCamera)
    {
        UE_LOG(LogCS, Warning, TEXT("SS Dummy: Remote camera is null"));
        return;
    }

    TargetCamera = RemoteCamera;
    bSyncDirectlyToCamera = true;
}

void ACSSpectatorPawn::SetTargetPawn(APawn* NewTargetPawn)
{
    if (!NewTargetPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SS Dummy: Target pawn is null"));
        TargetPawn = nullptr;
        TargetCamera = nullptr;
        return;
    }

    TargetPawn = NewTargetPawn;
    UpdateTargetCamera();
}

void ACSSpectatorPawn::UpdateTargetCamera()
{
    if (!TargetPawn) return;

    TargetCamera = SplitScreen::FindCameraInPawn(TargetPawn);

    if (TargetCamera)
    {
        if (bSyncDirectlyToCamera)
        {
            bHasTarget = false;
        }
    }
    else
    {
        UE_LOG(LogCS, Warning, TEXT("SS Dummy: No camera found in target pawn, using position sync"));
        bSyncDirectlyToCamera = false;
    }
}
