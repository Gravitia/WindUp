// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CSStagePortal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "Subsystem/CSGameProgressSubsystem.h"
#include "Settings/CSStageDataSettings.h"
#include "UI/CSStageSelectWidget.h"
#include "UI/CSViewFamilyViewportClient.h"

ACSStagePortal::ACSStagePortal()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    Light1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light1"));
    Light1->SetupAttachment(RootComponent);
    Light1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light1->SetVisibility(false);

    Light2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light2"));
    Light2->SetupAttachment(RootComponent);
    Light2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light2->SetVisibility(false);

    // Default to the fully code-built widget so the portal works with no UMG setup.
    // Override in the placed actor to use a custom Blueprint reparented to UCSStageSelectWidget.
    StageSelectWidgetClass = UCSStageSelectWidget::StaticClass();
}

void ACSStagePortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACSStagePortal, PlayerCount);
}

void ACSStagePortal::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACSStagePortal::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ACSStagePortal::OnOverlapEnd);

    ApplyLightVisibility();
}

void ACSStagePortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || bTransitionStarted) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character || !Character->IsPlayerControlled()) return;
    if (PlayersInTrigger.Contains(Character)) return;

    PlayersInTrigger.Add(Character);
    PlayerCount = PlayersInTrigger.Num();
    ApplyLightVisibility();

    if (PlayerCount >= RequiredPlayerCount && !bUIOpen)
    {
        bUIOpen = true;
        MulticastOpenStageSelect();
    }
}

void ACSStagePortal::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority() || bTransitionStarted) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    PlayersInTrigger.Remove(Character);
    PlayerCount = PlayersInTrigger.Num();
    ApplyLightVisibility();

    // Players broke the gathering before choosing — take the menu back down.
    if (bUIOpen && PlayerCount < RequiredPlayerCount)
    {
        bUIOpen = false;
        MulticastCloseStageSelect();
    }
}

void ACSStagePortal::OnRep_PlayerCount()
{
    ApplyLightVisibility();
}

void ACSStagePortal::ApplyLightVisibility()
{
    Light1->SetVisibility(PlayerCount >= 1);
    Light2->SetVisibility(PlayerCount >= 2);
}

void ACSStagePortal::MulticastOpenStageSelect_Implementation()
{
    OpenStageSelectUI();
}

void ACSStagePortal::MulticastCloseStageSelect_Implementation()
{
    CloseStageSelectUI();
}

void ACSStagePortal::MulticastBeginTransition_Implementation()
{
    // Take the menu down but keep input locked — we're about to travel.
    RemoveStageSelectWidget();

    UWorld* World = GetWorld();
    if (!World) return;

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        PC->DisableInput(PC);
    }

    if (UGameInstance* GI = World->GetGameInstance())
    {
        if (UCSViewFamilyViewportClient* VC = Cast<UCSViewFamilyViewportClient>(GI->GetGameViewportClient()))
        {
            VC->StartFade(1.0f, FadeDuration);
        }
    }
}

void ACSStagePortal::OpenStageSelectUI()
{
    if (ActiveWidget) return;

    if (!StageSelectWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("ACSStagePortal: StageSelectWidgetClass is not set"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->IsLocalController()) return;

    ActiveWidget = CreateWidget<UCSStageSelectWidget>(PC, StageSelectWidgetClass);
    if (!ActiveWidget) return;

    TArray<FCSStageOption> Options;
    BuildStageOptions(Options);

    ActiveWidget->Setup(this, Options);
    ActiveWidget->AddToViewport();

    // Stop the pawn cleanly: release any held keys (so EnhancedInput fires its
    // "released" events instead of leaving movement stuck), kill residual motion,
    // and block game input while the menu is up. Without this the character keeps
    // walking forward into/out of the trigger.
    PC->FlushPressedKeys();
    if (APawn* Pawn = PC->GetPawn())
    {
        Pawn->DisableInput(PC);
        if (UPawnMovementComponent* Move = Pawn->GetMovementComponent())
        {
            Move->StopMovementImmediately();
        }
    }

    PC->SetShowMouseCursor(true);
    PC->SetInputMode(FInputModeUIOnly());
}

void ACSStagePortal::CloseStageSelectUI()
{
    RemoveStageSelectWidget();

    UWorld* World = GetWorld();
    if (!World) return;

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->EnableInput(PC);
        }
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void ACSStagePortal::RemoveStageSelectWidget()
{
    if (ActiveWidget)
    {
        ActiveWidget->RemoveFromParent();
        ActiveWidget = nullptr;
    }
}

bool ACSStagePortal::ShouldAllowAllStages() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return bDebugUnlockAllStages;
#endif
}

void ACSStagePortal::BuildStageOptions(TArray<FCSStageOption>& OutOptions) const
{
    OutOptions.Reset();

    const UCSStageDataSettings* Data = UCSStageDataSettings::Get();
    if (!Data) return;

    int32 NextChapter = 0, NextStage = 0;
    const bool bHasNext = Data->GetNextStage(CurrentChapter, CurrentStage, NextChapter, NextStage);

    if (ShouldAllowAllStages())
    {
        // Debug: offer every stage in the table.
        for (const FCSChapterDef& Chapter : Data->Chapters)
        {
            for (const FCSStageDef& Stage : Chapter.Stages)
            {
                FCSStageOption Option;
                Option.Chapter = Chapter.Chapter;
                Option.Stage = Stage.Stage;
                Option.DisplayName = Data->GetStageDisplayName(Chapter.Chapter, Stage.Stage);
                Option.bSelectable = true;
                Option.bIsNextStage = bHasNext && Chapter.Chapter == NextChapter && Stage.Stage == NextStage;
                OutOptions.Add(Option);
            }
        }
    }
    else if (bHasNext)
    {
        // Normal play: the only destination is the next stage.
        FCSStageOption Option;
        Option.Chapter = NextChapter;
        Option.Stage = NextStage;
        Option.DisplayName = Data->GetStageDisplayName(NextChapter, NextStage);
        Option.bSelectable = true;
        Option.bIsNextStage = true;
        OutOptions.Add(Option);
    }
    // else: no next stage (end of game) and not debugging — empty list.
}

void ACSStagePortal::HandleTravelRequest(int32 Chapter, int32 Stage)
{
    if (!HasAuthority() || bTransitionStarted) return;

    // Validate the choice against what we'd actually offer.
    TArray<FCSStageOption> Options;
    BuildStageOptions(Options);
    const bool bValid = Options.ContainsByPredicate([Chapter, Stage](const FCSStageOption& O)
    {
        return O.Chapter == Chapter && O.Stage == Stage && O.bSelectable;
    });
    if (!bValid)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ACSStagePortal: rejected travel request to C%d_S%d (not a valid option)"),
            Chapter, Stage);
        return;
    }

    const UCSStageDataSettings* Data = UCSStageDataSettings::Get();
    const FString URL = Data ? Data->GetStageTravelURL(Chapter, Stage) : FString();
    if (URL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ACSStagePortal: C%d_S%d has no mapped level; aborting travel"), Chapter, Stage);
        return;
    }

    bTransitionStarted = true;
    PendingTravelURL = URL;

    if (UCSGameProgressSubsystem* Progress = GetProgressSubsystem())
    {
        if (bMarkCurrentStageCleared)
        {
            Progress->MarkStageCleared(CurrentChapter, CurrentStage);
        }
        Progress->SetLastPlayedStage(Chapter, Stage);
    }

    MulticastBeginTransition();

    FTimerHandle TravelTimer;
    GetWorldTimerManager().SetTimer(TravelTimer, this,
        &ACSStagePortal::DoServerTravel, FMath::Max(FadeDuration, 0.01f), false);
}

void ACSStagePortal::DoServerTravel()
{
    if (!HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->ServerTravel(PendingTravelURL, false);
    }
}

UCSGameProgressSubsystem* ACSStagePortal::GetProgressSubsystem() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        return GI->GetSubsystem<UCSGameProgressSubsystem>();
    }
    return nullptr;
}
