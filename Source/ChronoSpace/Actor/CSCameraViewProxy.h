// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CSCameraViewProxy.generated.h"

USTRUCT(BlueprintType)
struct FRepCamInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FOV = 90.f;
};

UCLASS()
class CHRONOSPACE_API ACSCameraViewProxy : public AActor
{
	GENERATED_BODY()
	
public:
    ACSCameraViewProxy();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 서버에서 카메라 소스가 될 PlayerController 지정 */
    UFUNCTION(BlueprintCallable, Category = "SS|CameraProxy")
    void SetSourcePC(APlayerController* InPC);

    /** 클라에서 읽을 수 있는 복제된 카메라 정보 */
    UFUNCTION(BlueprintPure, Category = "SS|CameraProxy")
    const FRepCamInfo& GetReplicatedCamera() const { return RepCam; }

protected:
    /** 복제되는 카메라 정보(서버가 채우고 클라가 읽음) */
    UPROPERTY(Replicated)
    FRepCamInfo RepCam;

    /** 서버에서만 의미 있는 소스 PC (클라에 복제되지 않음) */
    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> SourcePC;

public:
    UFUNCTION(Server, Reliable, WithValidation)   
    void ServerUpdateClientCamera(const FRepCamInfo& NewCam);

public:
    FORCEINLINE bool IsServerProxy() { return bIsServerProxy; }
    FORCEINLINE void SetIsServerProxy(bool bSeverProxy) { bIsServerProxy = bSeverProxy; }

protected:
    UPROPERTY(Replicated)
    bool bIsServerProxy;
};
