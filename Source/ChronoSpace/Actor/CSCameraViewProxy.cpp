// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSCameraViewProxy.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// Sets default values
ACSCameraViewProxy::ACSCameraViewProxy()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    bAlwaysRelevant = true;   // 어디서나 항상 관련
    NetUpdateFrequency = 60.f;   // 필요 시 조정
    SetReplicateMovement(false); // 우리는 위치/회전을 액터 위치로 안 쓰고, RepCam만 복제

    bIsServerProxy = false;

#if WITH_EDITOR
    SetActorLabel(TEXT("ACSCameraViewProxy"));
#endif
}

void ACSCameraViewProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACSCameraViewProxy, bIsServerProxy);
    DOREPLIFETIME(ACSCameraViewProxy, RepCam);
}

void ACSCameraViewProxy::SetSourcePC(APlayerController* InPC)
{
    if (!HasAuthority())
    {
        // 서버만 소스 설정 가능
        return;
    }

    SourcePC = InPC;
}

void ACSCameraViewProxy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        // *** 서버: Owner가 설정된 경우 (클라이언트 전용 Proxy) ***
        if (APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
        {
            // 이 Proxy는 특정 클라이언트 전용이므로 
            // 클라이언트가 RPC로 보낸 데이터만 복제하고, 서버에서 임의로 업데이트하지 않음
            // RepCam은 ServerUpdateClientCamera에서만 업데이트됨

            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Client Proxy for %s - RepCam: %s"),
                *OwnerPC->GetName(), *RepCam.Rotation.ToString());
        }
        // *** 서버: Owner가 없는 경우 (서버 전용 Proxy) ***
        else
        {
            // 서버 전용 Proxy - 리슨 서버(PlayerIndex 0)의 카메라 POV를 복제
            APlayerController* PC = SourcePC.Get();
            if (!PC)
            {
                if (UWorld* World = GetWorld())
                {
                    PC = UGameplayStatics::GetPlayerController(World, 0);
                    SourcePC = PC;
                }
            }

            if (PC && PC->PlayerCameraManager)
            {
                const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
                RepCam.Location = POV.Location;
                RepCam.Rotation = POV.Rotation;
                RepCam.FOV = POV.FOV;

                UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Server Proxy - RepCam: %s"),
                    *RepCam.Rotation.ToString());
            }
        }
    }
    else
    {
        static float SendTimer = 0.0f;
        SendTimer += DeltaSeconds;
        const float SendInterval = 1.f / 30.f;

        if (SendTimer >= SendInterval)
        {
            SendTimer = 0.f;

            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
                return;

            // 내 Proxy가 아닌 경우 호출 금지
            if (GetOwner() != PC)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Skipping RPC: Proxy owner (%s) != Local PC (%s)"),
                    *GetNameSafe(GetOwner()), *GetNameSafe(PC));
                return;
            }

            // 카메라 전송
            const FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCacheView();
            FRepCamInfo LocalCam;
            LocalCam.Location = POV.Location;
            LocalCam.Rotation = POV.Rotation;
            LocalCam.FOV = POV.FOV;

            ServerUpdateClientCamera(LocalCam);

            UE_LOG(LogTemp, VeryVerbose, TEXT("Client: Sending camera rotation (30Hz): %s"),
                *LocalCam.Rotation.ToString());
        }
    }
}

// client -> server 
void ACSCameraViewProxy::ServerUpdateClientCamera_Implementation(const FRepCamInfo& NewCam)
{
    // Owner 체크: 이 Proxy의 Owner 클라이언트만 업데이트 가능
    if (APlayerController* OwningPC = Cast<APlayerController>(GetOwner()))
    {
        // Owner가 RPC를 보낸 클라이언트와 일치하는지 확인
        if (OwningPC == GetNetConnection()->PlayerController)
        {
            RepCam = NewCam;
            UE_LOG(LogTemp, VeryVerbose, TEXT("SS Server: Updated client camera for %s - Rotation: %s"),
                *OwningPC->GetName(), *RepCam.Rotation.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SS Server: Camera update from wrong client!"));
        }
    }
    else
    {
        // Owner가 없는 서버 전용 Proxy는 클라이언트 RPC를 받으면 안됨
        UE_LOG(LogTemp, Warning, TEXT("SS Server: Received client camera update on server-only proxy!"));
    }
}

bool ACSCameraViewProxy::ServerUpdateClientCamera_Validate(const FRepCamInfo& NewCam)
{
    //if (!HasAuthority()) return false;
    // 여기서 데이터 유효성 검증 가능 (위치 값이 NaN인지, Rotation이 정상인지 등)
    return true; // 기본적으로 항상 허용
}