#include "CSStageGameInstanceSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

UCSStageGameInstanceSubsystem::UCSStageGameInstanceSubsystem()
{
    CurrentStage = "L_Main";  // 기본 스테이지 설정

    //  AbilityHUDClass가 NULL이면 기본 블루프린트 클래스 설정
    if (!AbilityHUDClass)
    {
        // /Script/UMGEditor.WidgetBlueprint'/Game/Blueprint/UI/BP_AbilityHUD.BP_AbilityHUD'
        static ConstructorHelpers::FClassFinder<UCSAbilityHUD> HUD_BP(TEXT("/Game/01_Blueprint/UI/BP_AbilityHUD"));
        if (HUD_BP.Succeeded())
        {
            AbilityHUDClass = HUD_BP.Class;
            UE_LOG(LogTemp, Warning, TEXT("AbilityHUDClass initialized from Blueprint!"));
        }
    }
}

void UCSStageGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    //  스테이지 어빌리티 데이터 초기화
    InitializeStageAbilities();

    UE_LOG(LogTemp, Warning, TEXT("CSStageGameInstanceSubsystem Initialized!"));
}

void UCSStageGameInstanceSubsystem::Deinitialize()
{
    Super::Deinitialize();
    UE_LOG(LogTemp, Warning, TEXT("CSStageGameInstanceSubsystem Deinitialized!"));
}

void UCSStageGameInstanceSubsystem::ChangeStage(FString NewStage)
{
    UE_LOG(LogTemp, Warning, TEXT("ChangeStage called with: %s"), *NewStage);

    CurrentStage = NewStage;
    OnStageChanged.Broadcast();

    UWorld* World = GetWorld();
    if (!World) return;

    // OpenLevel 은 로컬 트래블이다 - 리슨 서버에서 부르면 접속한 클라가 전부 끊기고,
    // 클라에서 부르면 그 클라만 세션을 떠난다. 코옵에서는 서버 권한 + ServerTravel 이어야 한다.
    if (!World->GetAuthGameMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("ChangeStage: server only (ignored on client) - %s"), *NewStage);
        return;
    }

    World->ServerTravel(NewStage + TEXT("?listen"), false);
} 

TArray<FString> UCSStageGameInstanceSubsystem::GetAvailableAbilities()
{
    if (StageAbilities.Contains(CurrentStage))
    {
        return StageAbilities[CurrentStage].Abilities;
    }
    return TArray<FString>();
}

void UCSStageGameInstanceSubsystem::InitializeStageAbilities()
{
    //  예제 데이터 추가
    FStageAbilityList Stage1Abilities;
    Stage1Abilities.Abilities = { "ReverseGravity", "TimeStop" };

    FStageAbilityList Stage2Abilities;
    Stage2Abilities.Abilities = { "TimeStop" };

    StageAbilities.Add("L_Stage1", Stage1Abilities);
    StageAbilities.Add("L_Stage2", Stage2Abilities);
}
