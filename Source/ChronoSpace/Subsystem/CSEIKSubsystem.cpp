// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSEIKSubsystem.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "ChronoSpace.h"

void UCSEIKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoginWithDeviceId();
}

void UCSEIKSubsystem::Deinitialize()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem)
    {
        UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
        return;
    }

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Invalid IOnlineIdentityPtr"));
        return;
    }

    Identity->Logout(0);

    Super::Deinitialize();
}

void UCSEIKSubsystem::LoginWithDeviceId()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem)
    {
        UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
        return;
    }

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Invalid IOnlineIdentityPtr"));
        return;
    }

    FString RandomID = FGuid::NewGuid().ToString();

    FOnlineAccountCredentials Creds;
    Creds.Type = TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN");
    Creds.Id = RandomID;
    Creds.Token = TEXT("");

    Identity->OnLoginCompleteDelegates->AddUObject(
        this,
        &UCSEIKSubsystem::OnLoginComplete
    );

    Identity->Login(0, Creds);
}

void UCSEIKSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    if (bWasSuccessful)
    {
        UE_LOG(LogCS, Log, TEXT("[UCSAuthSubsystem] DeviceID Login Success: %s"), *UserId.ToString());
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("[UCSAuthSubsystem] Login Failed: %s"), *Error);
    }
}

void UCSEIKSubsystem::CreateSession()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem)
    {
        UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
        return;
    }

    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (!Session.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Invalid IOnlineSessionPtr"));
        return;
    }

    OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(
        this,
        &UCSEIKSubsystem::OnCreateSessionComplete
    );

    OnCreateSessionCompleteDelegateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = false;
    Settings.NumPublicConnections = 2;
    Settings.bShouldAdvertise = true;
    Settings.bUsesPresence = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bIsDedicated = false;
    Settings.Set(FName("MAPNAME"), FString("L_StageSize"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    Session->CreateSession(0, NAME_GameSession, Settings);
}

void UCSEIKSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    Session->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);

    if (bWasSuccessful)
    {
        UE_LOG(LogCS, Log, TEXT("Create Session Success: %s"), *SessionName.ToString());

        OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(
            this,
            &UCSEIKSubsystem::OnStartSessionComplete
        );
        OnStartSessionCompleteDelegateHandle = Session->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);

        Session->StartSession(NAME_GameSession);

    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Create Session Failed.."));
    }
}

void UCSEIKSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    Session->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);

    if (!bWasSuccessful)
    {
        UE_LOG(LogCS, Error, TEXT("Start Session Failed"));
        return;
    }

    UWorld* World = GetWorld();

    //UE_LOG(LogCS, Log, TEXT("NetDriver: %s"), *World->GetNetDriver()->NetDriverName.ToString());

    if (World && World->GetAuthGameMode())
    {
        UE_LOG(LogCS, Log, TEXT("ServerTravel - L_StageSize"));
        World->ServerTravel("/Game/02_Map/L_StageSize?listen");
    }
}

void UCSEIKSubsystem::FindSessions()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem)
    {
        UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
        return;
    }

    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (!Session.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Invalid IOnlineSessionPtr"));
        return;
    }

    SessionSearch = MakeShareable(new FOnlineSessionSearch);
    SessionSearch->MaxSearchResults = 20;
    SessionSearch->bIsLanQuery = false;
    SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

    OnFindSessionCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(
        this,
        &UCSEIKSubsystem::OnFindSessionsComplete
    );
    OnFindSessionCompleteDelegateHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegate);

    Session->FindSessions(0, SessionSearch.ToSharedRef());
}

void UCSEIKSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegateHandle);

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
        {
            FString SessionName = SearchResult.GetSessionIdStr();
            UE_LOG(LogCS, Log, TEXT("Find Session Success: %s"), *SessionName);
            JoinSession(SearchResult);
            break;
        }
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Find Session Failed.."));
    }
}

void UCSEIKSubsystem::JoinSessionForBlueprint(FBlueprintSessionResult& SearchResult)
{
}

void UCSEIKSubsystem::JoinSession(const FOnlineSessionSearchResult& SearchResult)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem)
    {
        UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
        return;
    }

    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (!Session.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Invalid IOnlineSessionPtr"));
        return;
    }

    OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(
        this,
        &UCSEIKSubsystem::OnJoinSessionComplete
    );
    OnJoinSessionCompleteDelegateHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

    Session->JoinSession(0, NAME_GameSession, SearchResult);
}

void UCSEIKSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    Session->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

    FString ConnectInfo;
    if (Session->GetResolvedConnectString(SessionName, ConnectInfo))
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
        }
    }

    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogCS, Log, TEXT("Join Session Success: %s"), *SessionName.ToString());
    }
    else
    {
        UE_LOG(LogCS, Error, TEXT("Join Session Failed: %d"), (int32)Result);
    }
}