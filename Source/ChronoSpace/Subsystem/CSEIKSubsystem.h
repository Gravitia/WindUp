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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCSJoinSessionFailedSignature, int32, ResultCode);

UCLASS()
class CHRONOSPACE_API UCSEIKSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 세션 참가 실패 알림 (Result 코드). UI 가 구독해 사용자에게 알린다 - 예전엔 실패해도 아무 신호가 없었다. */
	UPROPERTY(BlueprintAssignable)
	FCSJoinSessionFailedSignature OnJoinSessionFailed;

	/** 세션 시작 후 호스트가 이동할 맵 (예: /Game/02_Map/L_Stage01). 비우면 트래블하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "CSEditable|Session")
	FString LobbyMapPath;

protected:
	/** 로그인에 성공한 적이 있는가 - Deinitialize 에서 불필요한 Logout 을 피한다 */
	bool bLoggedIn = false;

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

	// Start Session
protected:
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
	FDelegateHandle OnStartSessionCompleteDelegateHandle;

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

	void JoinSession(const FOnlineSessionSearchResult& SearchResult);   // FOnlineSessionSearchResult는 리플렉션 지원이 안됨

protected:
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;
	FDelegateHandle OnJoinSessionCompleteDelegateHandle;


};