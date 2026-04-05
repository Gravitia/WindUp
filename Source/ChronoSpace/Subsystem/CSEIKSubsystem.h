// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FindSessionsCallbackProxy.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CSEIKSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSEIKLoginComplete, bool, bWasSuccessful, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSEIKCreateSessionComplete, bool, bWasSuccessful, FName, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSEIKFindSessionsComplete, bool, bWasSuccessful, const TArray<FBlueprintSessionResult>&, SearchResults);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSEIKJoinSessionComplete, bool, bWasSuccessful, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSEIKDestroySessionComplete, bool, bWasSuccessful, FName, SessionName);

UCLASS()
class CHRONOSPACE_API UCSEIKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "EIK")
	FCSEIKLoginComplete OnLoginCompleted;

	UPROPERTY(BlueprintAssignable, Category = "EIK")
	FCSEIKCreateSessionComplete OnCreateSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "EIK")
	FCSEIKFindSessionsComplete OnFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "EIK")
	FCSEIKJoinSessionComplete OnJoinSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "EIK")
	FCSEIKDestroySessionComplete OnDestroySessionCompleted;

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void LoginWithDeviceId(const FString& DisplayName = TEXT("Player"), const FString& DeviceName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "EIK")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void CreateSession(int32 PublicConnections = 2, const FString& MapName = TEXT("L_StageSize"), const FString& RoomName = TEXT("DefaultRoom"));

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void FindOrCreateSession(int32 PublicConnections = 2, const FString& MapName = TEXT("L_StageSize"), const FString& RoomName = TEXT("DefaultRoom"));

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void DestroySession();

	UFUNCTION(BlueprintPure, Category = "EIK")
	bool HasActiveNamedSession() const;

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void FindSessions(const FString& RoomName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "EIK")
	TArray<FBlueprintSessionResult> GetCachedSearchResults() const;

	UFUNCTION(BlueprintCallable, Category = "EIK")
	void JoinSessionForBlueprint(const FBlueprintSessionResult& SearchResult);

	UFUNCTION(BlueprintCallable, Category = "EIK")
	bool JoinSessionByIndex(int32 SearchResultIndex);

	void JoinSession(const FOnlineSessionSearchResult& SearchResult);

protected:
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FDelegateHandle OnCreateSessionCompleteDelegateHandle;

	FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
	FDelegateHandle OnStartSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegateNative;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FBlueprintSessionResult> CachedSearchResults;

	FOnFindSessionsCompleteDelegate OnFindSessionCompleteDelegate;
	FDelegateHandle OnFindSessionCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegateNative;
	FDelegateHandle OnJoinSessionCompleteDelegateHandle;

private:
	IOnlineSubsystem* GetEIKSubsystem() const;
	IOnlineIdentityPtr GetIdentityInterface() const;
	IOnlineSessionPtr GetSessionInterface() const;
	void ResetSearchState();
	void CacheSearchResults();
	bool CanStartSessionOperation(const TCHAR* OperationName);
	void CreateSessionInternal();
	bool TryJoinBestSearchResult();

	int32 PendingPublicConnections = 2;
	FString PendingMapName = TEXT("L_StageSize");
	FString PendingRoomName = TEXT("DefaultRoom");
	bool bCreateAfterDestroy = false;
	bool bFindAfterDestroy = false;
	bool bAutoCreateAfterFind = false;
	bool bOperationInFlight = false;
	FName ActiveSessionName = NAME_GameSession;
};
