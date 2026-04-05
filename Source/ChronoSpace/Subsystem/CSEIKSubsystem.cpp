// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSEIKSubsystem.h"

#include "ChronoSpace.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"

namespace
{
	FString JoinResultToString(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:
			return TEXT("Success");
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return TEXT("SessionIsFull");
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return TEXT("SessionDoesNotExist");
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return TEXT("CouldNotRetrieveAddress");
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return TEXT("AlreadyInSession");
		case EOnJoinSessionCompleteResult::UnknownError:
		default:
			return TEXT("UnknownError");
		}
	}
}

void UCSEIKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogCS, Log, TEXT("CSEIKSubsystem initialized"));
}

void UCSEIKSubsystem::Deinitialize()
{
	if (HasActiveNamedSession())
	{
		DestroySession();
	}

	Super::Deinitialize();
}

void UCSEIKSubsystem::LoginWithDeviceId(const FString& DisplayName, const FString& DeviceName)
{
	IOnlineIdentityPtr Identity = GetIdentityInterface();
	if (!Identity.IsValid())
	{
		OnLoginCompleted.Broadcast(false, TEXT("Failed to get EIK identity interface"));
		return;
	}

	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		OnLoginCompleted.Broadcast(true, FString());
		return;
	}

	FOnlineAccountCredentials Creds;
	Creds.Type = TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN");
	Creds.Id = DisplayName.IsEmpty() ? TEXT("Player") : DisplayName;
	Creds.Token = TEXT("");

	Identity->OnLoginCompleteDelegates->AddUObject(this, &UCSEIKSubsystem::OnLoginComplete);
	Identity->Login(0, Creds);
}

bool UCSEIKSubsystem::IsLoggedIn() const
{
	IOnlineIdentityPtr Identity = GetIdentityInterface();
	return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

void UCSEIKSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogCS, Log, TEXT("EIK login succeeded: %s"), *UserId.ToString());
		OnLoginCompleted.Broadcast(true, FString());
		return;
	}

	UE_LOG(LogCS, Error, TEXT("EIK login failed: %s"), *Error);
	OnLoginCompleted.Broadcast(false, Error);
}

void UCSEIKSubsystem::CreateSession(int32 PublicConnections, const FString& MapName, const FString& RoomName)
{
	PendingPublicConnections = FMath::Max(1, PublicConnections);
	PendingMapName = MapName.IsEmpty() ? TEXT("L_StageSize") : MapName;
	PendingRoomName = RoomName.IsEmpty() ? TEXT("DefaultRoom") : RoomName;

	if (!CanStartSessionOperation(TEXT("CreateSession")))
	{
		return;
	}

	if (HasActiveNamedSession())
	{
		bCreateAfterDestroy = true;
		DestroySession();
		return;
	}

	CreateSessionInternal();
}

void UCSEIKSubsystem::FindOrCreateSession(int32 PublicConnections, const FString& MapName, const FString& RoomName)
{
	PendingPublicConnections = FMath::Max(1, PublicConnections);
	PendingMapName = MapName.IsEmpty() ? TEXT("L_StageSize") : MapName;
	PendingRoomName = RoomName.IsEmpty() ? TEXT("DefaultRoom") : RoomName;
	bAutoCreateAfterFind = true;
	FindSessions(PendingRoomName);
}

void UCSEIKSubsystem::DestroySession()
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnDestroySessionCompleted.Broadcast(false, ActiveSessionName);
		return;
	}

	FNamedOnlineSession* ExistingSession = Session->GetNamedSession(ActiveSessionName);
	if (!ExistingSession)
	{
		OnDestroySessionCompleted.Broadcast(true, ActiveSessionName);
		return;
	}

	OnDestroySessionCompleteDelegateNative = FOnDestroySessionCompleteDelegate::CreateUObject(
		this,
		&UCSEIKSubsystem::OnDestroySessionComplete
	);
	OnDestroySessionCompleteDelegateHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateNative);

	if (!Session->DestroySession(ActiveSessionName))
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		OnDestroySessionCompleted.Broadcast(false, ActiveSessionName);
	}
}

bool UCSEIKSubsystem::HasActiveNamedSession() const
{
	IOnlineSessionPtr Session = GetSessionInterface();
	return Session.IsValid() && Session->GetNamedSession(ActiveSessionName) != nullptr;
}

void UCSEIKSubsystem::CreateSessionInternal()
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnCreateSessionCompleted.Broadcast(false, ActiveSessionName);
		return;
	}

	OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(
		this,
		&UCSEIKSubsystem::OnCreateSessionComplete
	);
	OnCreateSessionCompleteDelegateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = false;
	Settings.NumPublicConnections = PendingPublicConnections;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bUsesPresence = false;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bAllowInvites = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.bIsDedicated = false;
	Settings.Set(SEARCH_KEYWORDS, FString(ActiveSessionName.ToString()), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("MAPNAME")), PendingMapName, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("ROOMNAME")), PendingRoomName, EOnlineDataAdvertisementType::ViaOnlineService);

	bOperationInFlight = true;
	if (!Session->CreateSession(0, ActiveSessionName, Settings))
	{
		bOperationInFlight = false;
		Session->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		OnCreateSessionCompleted.Broadcast(false, ActiveSessionName);
	}
}

void UCSEIKSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
	}

	if (!bWasSuccessful || !Session.IsValid())
	{
		bOperationInFlight = false;
		UE_LOG(LogCS, Error, TEXT("Create session failed: %s"), *SessionName.ToString());
		OnCreateSessionCompleted.Broadcast(false, SessionName);
		return;
	}

	UE_LOG(LogCS, Log, TEXT("Create session succeeded: %s"), *SessionName.ToString());
	OnCreateSessionCompleted.Broadcast(true, SessionName);

	OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(
		this,
		&UCSEIKSubsystem::OnStartSessionComplete
	);
	OnStartSessionCompleteDelegateHandle = Session->AddOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);

	if (!Session->StartSession(SessionName))
	{
		bOperationInFlight = false;
		Session->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
		UE_LOG(LogCS, Error, TEXT("Start session request failed: %s"), *SessionName.ToString());
	}
}

void UCSEIKSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
	}

	bOperationInFlight = false;

	if (!bWasSuccessful)
	{
		UE_LOG(LogCS, Error, TEXT("Start session failed: %s"), *SessionName.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetAuthGameMode())
	{
		const FString TravelUrl = FString::Printf(TEXT("/Game/02_Map/%s?listen"), *PendingMapName);
		UE_LOG(LogCS, Log, TEXT("ServerTravel: %s"), *TravelUrl);
		World->ServerTravel(TravelUrl);
	}
}

void UCSEIKSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
	}

	UE_LOG(LogCS, Log, TEXT("Destroy session finished: %s (%s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("Success") : TEXT("Fail"));
	OnDestroySessionCompleted.Broadcast(bWasSuccessful, SessionName);

	if (!bWasSuccessful)
	{
		bCreateAfterDestroy = false;
		bFindAfterDestroy = false;
		bOperationInFlight = false;
		return;
	}

	if (bCreateAfterDestroy)
	{
		bCreateAfterDestroy = false;
		CreateSessionInternal();
		return;
	}

	if (bFindAfterDestroy)
	{
		bFindAfterDestroy = false;
		FindSessions(PendingRoomName);
		return;
	}

	bOperationInFlight = false;
}

void UCSEIKSubsystem::FindSessions(const FString& RoomName)
{
	if (!CanStartSessionOperation(TEXT("FindSessions")))
	{
		return;
	}

	if (!RoomName.IsEmpty())
	{
		PendingRoomName = RoomName;
	}

	if (HasActiveNamedSession())
	{
		bFindAfterDestroy = true;
		DestroySession();
		return;
	}

	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnFindSessionsCompleted.Broadcast(false, CachedSearchResults);
		return;
	}

	ResetSearchState();
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	if (!PendingRoomName.IsEmpty())
	{
		SessionSearch->QuerySettings.Set(FName(TEXT("ROOMNAME")), PendingRoomName, EOnlineComparisonOp::Equals);
	}

	UE_LOG(
		LogCS,
		Log,
		TEXT("FindSessions query started. RoomName=%s, UsesPresence=false, UsesLobbies=true"),
		*PendingRoomName
	);

	OnFindSessionCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(
		this,
		&UCSEIKSubsystem::OnFindSessionsComplete
	);
	OnFindSessionCompleteDelegateHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegate);

	bOperationInFlight = true;
	if (!Session->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		bOperationInFlight = false;
		Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegateHandle);
		OnFindSessionsCompleted.Broadcast(false, CachedSearchResults);
	}
}

TArray<FBlueprintSessionResult> UCSEIKSubsystem::GetCachedSearchResults() const
{
	return CachedSearchResults;
}

void UCSEIKSubsystem::ResetSearchState()
{
	CachedSearchResults.Reset();
}

void UCSEIKSubsystem::CacheSearchResults()
{
	CachedSearchResults.Reset();

	if (!SessionSearch.IsValid())
	{
		return;
	}

	for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
	{
		if (!Result.IsValid())
		{
			continue;
		}

		const int32 OpenConnections = Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections;
		if (OpenConnections <= 0)
		{
			continue;
		}

		FBlueprintSessionResult BlueprintResult;
		BlueprintResult.OnlineResult = Result;

		// UE 5.5 + EIK sometimes returns this incompletely for lobby joins.
		if (!BlueprintResult.OnlineResult.Session.SessionSettings.bIsDedicated)
		{
			BlueprintResult.OnlineResult.Session.SessionSettings.bUsesPresence = false;
			BlueprintResult.OnlineResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		}

		CachedSearchResults.Add(BlueprintResult);
	}
}

void UCSEIKSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegateHandle);
	}

	bOperationInFlight = false;
	CacheSearchResults();

	UE_LOG(LogCS, Log, TEXT("Find sessions finished: %s, %d result(s)"), bWasSuccessful ? TEXT("Success") : TEXT("Fail"), CachedSearchResults.Num());
	OnFindSessionsCompleted.Broadcast(bWasSuccessful, CachedSearchResults);

	if (!bAutoCreateAfterFind)
	{
		return;
	}

	bAutoCreateAfterFind = false;
	if (TryJoinBestSearchResult())
	{
		return;
	}

	CreateSession(PendingPublicConnections, PendingMapName, PendingRoomName);
}

bool UCSEIKSubsystem::TryJoinBestSearchResult()
{
	if (CachedSearchResults.Num() <= 0)
	{
		return false;
	}

	for (const FBlueprintSessionResult& SearchResult : CachedSearchResults)
	{
		const FOnlineSession& SessionData = SearchResult.OnlineResult.Session;
		const int32 OpenConnections = SessionData.NumOpenPublicConnections + SessionData.NumOpenPrivateConnections;
		if (OpenConnections > 0)
		{
			JoinSessionForBlueprint(SearchResult);
			return true;
		}
	}

	return false;
}

void UCSEIKSubsystem::JoinSessionForBlueprint(const FBlueprintSessionResult& SearchResult)
{
	if (!SearchResult.OnlineResult.IsSessionInfoValid())
	{
		OnJoinSessionCompleted.Broadcast(false, FString());
		return;
	}

	JoinSession(SearchResult.OnlineResult);
}

bool UCSEIKSubsystem::JoinSessionByIndex(int32 SearchResultIndex)
{
	if (!CachedSearchResults.IsValidIndex(SearchResultIndex))
	{
		return false;
	}

	JoinSessionForBlueprint(CachedSearchResults[SearchResultIndex]);
	return true;
}

void UCSEIKSubsystem::JoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	if (!CanStartSessionOperation(TEXT("JoinSession")))
	{
		return;
	}

	if (HasActiveNamedSession())
	{
		bCreateAfterDestroy = false;
		bFindAfterDestroy = false;
		bAutoCreateAfterFind = false;
		OnJoinSessionCompleted.Broadcast(false, TEXT("Local session already exists. Destroy it first."));
		return;
	}

	IOnlineSessionPtr Session = GetSessionInterface();
	if (!Session.IsValid())
	{
		OnJoinSessionCompleted.Broadcast(false, TEXT("Failed to get EIK session interface"));
		return;
	}

	OnJoinSessionCompleteDelegateNative = FOnJoinSessionCompleteDelegate::CreateUObject(
		this,
		&UCSEIKSubsystem::OnJoinSessionComplete
	);
	OnJoinSessionCompleteDelegateHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateNative);

	bOperationInFlight = true;
	if (!Session->JoinSession(0, ActiveSessionName, SearchResult))
	{
		bOperationInFlight = false;
		Session->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
		OnJoinSessionCompleted.Broadcast(false, TEXT("JoinSession request failed"));
	}
}

void UCSEIKSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Session = GetSessionInterface();
	if (Session.IsValid())
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);
	}

	bOperationInFlight = false;

	if (!Session.IsValid() || Result != EOnJoinSessionCompleteResult::Success)
	{
		const FString ResultString = JoinResultToString(Result);
		UE_LOG(LogCS, Error, TEXT("Join session failed: %s"), *ResultString);
		OnJoinSessionCompleted.Broadcast(false, FString::Printf(TEXT("Join failed: %s"), *ResultString));
		return;
	}

	FString ConnectInfo;
	if (!Session->GetResolvedConnectString(SessionName, ConnectInfo))
	{
		OnJoinSessionCompleted.Broadcast(false, TEXT("Join failed: resolved connect string is empty"));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PlayerController)
	{
		OnJoinSessionCompleted.Broadcast(false, TEXT("Join failed: PlayerController not found"));
		return;
	}

	PlayerController->ClientTravel(ConnectInfo, TRAVEL_Absolute);
	UE_LOG(LogCS, Log, TEXT("Join session succeeded: %s"), *ConnectInfo);
	OnJoinSessionCompleted.Broadcast(true, FString::Printf(TEXT("Join succeeded: %s"), *ConnectInfo));
}

IOnlineSubsystem* UCSEIKSubsystem::GetEIKSubsystem() const
{
	if (IOnlineSubsystem* EIKSubsystem = IOnlineSubsystem::Get(TEXT("EIK")))
	{
		return EIKSubsystem;
	}

	return IOnlineSubsystem::Get();
}

IOnlineIdentityPtr UCSEIKSubsystem::GetIdentityInterface() const
{
	if (IOnlineSubsystem* Subsystem = GetEIKSubsystem())
	{
		return Subsystem->GetIdentityInterface();
	}

	return nullptr;
}

IOnlineSessionPtr UCSEIKSubsystem::GetSessionInterface() const
{
	if (IOnlineSubsystem* Subsystem = GetEIKSubsystem())
	{
		return Subsystem->GetSessionInterface();
	}

	return nullptr;
}

bool UCSEIKSubsystem::CanStartSessionOperation(const TCHAR* OperationName)
{
	if (bOperationInFlight)
	{
		UE_LOG(LogCS, Warning, TEXT("%s ignored: another session operation is in flight"), OperationName);
		return false;
	}

	return true;
}
