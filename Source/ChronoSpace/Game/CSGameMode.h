// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Actor/System/CSRespawnPoint.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Player/CSPlayerState.h" // ECSPlayerSlot
#include "CSGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerLogin);


/**
 *
 */
UCLASS()
class CHRONOSPACE_API ACSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    ACSGameMode();
    FOnPlayerLogin OnPlayerLogin;

    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer0;

    UPROPERTY(EditDefaultsOnly, Category = "Pawn")
    TSubclassOf<APawn> PawnClassPlayer1;

    bool RespawnSinglePlayer(APawn* Player);

    /**
     * 슬롯 -> 폰 클래스. GetDefaultPawnClassForController 가 참조하는 것과 같은 표를,
     * 컨트롤러 없이 슬롯만으로 물어볼 수 있게 열어 둔 창구다.
     *
     * 게임플레이 경로는 계속 GetDefaultPawnClassForController 를 쓴다. 이 함수는
     * ACSPlayerController 가 "지금 잡고 있는 몸이 내 슬롯의 몸인가" 를 판정하는 데 쓴다
     * (디버그 캐릭터 스왑 표시). 폰 클래스를 정하는 규칙을 여기서 새로 만들지 말 것 —
     * 표는 한 곳에만 있어야 한다.
     */
    TSubclassOf<APawn> GetPawnClassForSlot(ECSPlayerSlot Slot) const;

    /**
     * 이 폰이 "어느 슬롯의 몸" 인지 판정한다. 성공하면 bOutResolved = true.
     *
     * 조작자가 아니라 *몸* 을 본다. 디버그 캐릭터 스왑으로 컨트롤러가 남의 몸을 잡고 있어도
     * 이 함수는 그 몸의 원래 신원을 돌려준다.
     * (APawn::PlayerState 는 빙의를 따라가므로 몸의 신원 판정에 쓸 수 없다 —
     *  ACSPlayerController.h 의 "Debug — 다른 플레이어와 조작 캐릭터 맞바꾸기" 주석 참고)
     *
     * 클라이언트에서도 부를 수 있다. AGameStateBase::GameModeClass 가 복제되고
     * (GameStateBase.cpp 의 DOREPLIFETIME_CONDITION(..., COND_InitialOnly)),
     * GetDefaultGameMode<T>() 가 그 CDO 를 준다. PawnClassPlayer0/1 은 EditDefaultsOnly 라
     * CDO 에 값이 들어 있다.
     *
     * 접속 직후 GameState 첫 번치가 오기 전에는 실패한다. 그 결과를 캐시하지 말 것 —
     * 캐시하면 그 창은 영원히 판정 불가로 남는다. 매 프레임 다시 물어라.
     *
     * ★ IsA / IsChildOf 를 쓰지 않고 클래스를 *정확히* 비교한다.
     *   BPC_CharacterPlayer_02(P2 몸)의 부모가 BP_CharacterPlayer(P1 몸)라서,
     *   IsA 로 판정하면 두 몸이 모두 Player0 으로 나온다.
     */
    static ECSPlayerSlot ResolveBodySlotForPawn(const UWorld* World, const APawn* InPawn, bool& bOutResolved);

protected:
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
    /** SeamlessTravel 시, 기존 PlayerController가 옴겨질 때 호출됨 */
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    class ACSGameState* GetCSGameState() const;


// Split Screen
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY() // GC 보호
    TObjectPtr< class ACSCameraViewProxy > ServerCamProxy = nullptr;

    UPROPERTY()
    TMap< TObjectPtr< APlayerController >, TObjectPtr< ACSCameraViewProxy > > ClientCamProxies;

private:
    TArray< TObjectPtr< APlayerController > > ConnectedPlayers;

    /** PostLogin/SeamlessTravel 공통: 플레이어별 CameraViewProxy 생성 */
    void CreateProxiesForPlayer(APlayerController* NewPlayer);

    /** PostLogin/SeamlessTravel 공통: SplitScreen 조건 확인 + Subsystem Enable */
    void TrySplitScreenSetup();

    /** Logout: 나간 플레이어의 Proxy 리소스 정리 */
    void CleanupSplitScreenForPlayer(APlayerController* ExitingPlayer);
};
