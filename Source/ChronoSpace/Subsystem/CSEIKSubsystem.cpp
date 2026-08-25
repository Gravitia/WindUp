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
    // 예전엔 첫 줄이 return; 이라 Super::Initialize 조차 도달하지 못했다.
    // 자동 로그인은 여전히 하지 않는다 - 로그인/세션 생성은 BP 가 명시적으로 호출한다.
    Super::Initialize(Collection);
}

void UCSEIKSubsystem::Deinitialize()
{
    // 어떤 경로로 빠져나가든 Super::Deinitialize 는 반드시 호출해야 한다.
    if (bLoggedIn)
    {
        if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK")))
        {
            IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
            if (Identity.IsValid())
            {
                // 로그인한 적이 있을 때만 로그아웃한다
                Identity->ClearOnLoginCompleteDelegates(0, this);
                Identity->Logout(0);
            }
        }
        bLoggedIn = false;
    }

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

    // 호출할 때마다 델리게이트를 더하면 로그인 버튼을 두 번 누를 때 콜백도 두 번 온다.
    // 기존 바인딩을 먼저 지운다.
    Identity->ClearOnLoginCompleteDelegates(0, this);
    Identity->OnLoginCompleteDelegates->AddUObject(
        this,
        &UCSEIKSubsystem::OnLoginComplete
    );

    Identity->Login(0, Creds);
}

void UCSEIKSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    bLoggedIn = bWasSuccessful;

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
    // 종료 중이거나 EIK 가 초기화되지 않았으면 여기로 null 이 온다 - 무검사 역참조는 크래시
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (!Session.IsValid()) return;

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
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (!Session.IsValid()) return;

    Session->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);

    if (!bWasSuccessful)
    {
        UE_LOG(LogCS, Error, TEXT("Start Session Failed"));
        return;
    }

    // 예전엔 "/Game/02_Map/L_StageSize" 를 하드코딩했는데 그 맵은 존재하지 않아
    // 세션은 만들어지고 트래블만 조용히 실패했다. 이제 에디터에서 지정한 맵을 쓰고, 비어 있으면 에러를 남긴다.
    if (LobbyMapPath.IsEmpty())
    {
        UE_LOG(LogCS, Error, TEXT("OnStartSessionComplete: LobbyMapPath is empty - set it in the subsystem defaults"));
        return;
    }

    UWorld* World = GetWorld();
    if (World && World->GetAuthGameMode())
    {
        UE_LOG(LogCS, Log, TEXT("ServerTravel - %s"), *LobbyMapPath);
        World->ServerTravel(LobbyMapPath + TEXT("?listen"));
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
    SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);

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
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (!Session.IsValid()) return;

    Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegateHandle);

    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        UE_LOG(LogCS, Error, TEXT("Find Session Failed.."));
        return;
    }

    // 검색 결과가 0건인 것과 실패는 다르다 - 예전엔 0건일 때 아무 로그도 남지 않아 원인을 알 수 없었다
    if (SessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogCS, Warning, TEXT("Find Session: no sessions found"));
        return;
    }

    const FOnlineSessionSearchResult& SearchResult = SessionSearch->SearchResults[0];
    UE_LOG(LogCS, Log, TEXT("Find Session Success: %s (%d found)"), *SearchResult.GetSessionIdStr(), SessionSearch->SearchResults.Num());
    JoinSession(SearchResult);
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
    IOnlineSessionPtr Session = Subsystem ? Subsystem->GetSessionInterface() : nullptr;
    if (!Session.IsValid()) return;

    Session->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

    // 결과를 먼저 본다. 예전엔 실패(세션 가득 참, 만료된 검색 결과)여도
    // 이전 세션 정보로 ClientTravel 을 시도해 접속 실패 화면으로 떨어졌다.
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogCS, Error, TEXT("Join Session Failed: %d"), (int32)Result);
        OnJoinSessionFailed.Broadcast((int32)Result);
        return;
    }

    UE_LOG(LogCS, Log, TEXT("Join Session Success: %s"), *SessionName.ToString());

    FString ConnectInfo;
    if (!Session->GetResolvedConnectString(SessionName, ConnectInfo))
    {
        UE_LOG(LogCS, Error, TEXT("Join Session: could not resolve connect string"));
        OnJoinSessionFailed.Broadcast((int32)EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
    }
}
