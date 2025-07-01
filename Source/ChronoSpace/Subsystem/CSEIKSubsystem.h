// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "CSEIKSubsystem.generated.h"

/**
 *
 */
UCLASS()
class CHRONOSPACE_API UCSEIKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Login
public:
	UFUNCTION(BlueprintCallable)
	void LoginWithDeviceId();

protected:
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	// Create Session
public:
	UFUNCTION(BlueprintCallable)
	void CreateSession();

protected:
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FDelegateHandle OnCreateSessionCompleteDelegateHandle;

	// Find Session
public:
	UFUNCTION(BlueprintCallable)
	void FindSessions();

protected:
	void OnFindSessionsComplete(bool bWasSuccessful);

	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FOnFindSessionsCompleteDelegate OnFindSessionCompleteDelegate;
	FDelegateHandle OnFindSessionCompleteDelegateHandle;

	// Join Session
public:
	UFUNCTION(BlueprintCallable)
	void JoinSessionForBlueprint(FBlueprintSessionResult& SearchResult);

	void JoinSession(const FOnlineSessionSearchResult& SearchResult);	// FOnlineSessionSearchResult는 리플렉션 지원이 안됨

protected:
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;
	FDelegateHandle OnJoinSessionCompleteDelegateHandle;


};