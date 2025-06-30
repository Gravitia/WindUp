// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSAuthSubsystem.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "ChronoSpace.h"

void UCSAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoginWithDeviceId();
}

void UCSAuthSubsystem::Deinitialize()
{
	
	
	Super::Deinitialize();
}

void UCSAuthSubsystem::LoginWithDeviceId()
{
	// for EOS
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
	if (!Subsystem)
	{
		UE_LOG(LogCS, Error, TEXT("No EIK Subsystem"));
		return;
	}
	//UE_LOG(LogCS, Log, TEXT("Valid EOS Subsystem"));

	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	if ( !Identity.IsValid() )
	{
		UE_LOG(LogCS, Error, TEXT("Invalid IOnlineIdentityPtr"));
		return;
	}

	FOnlineAccountCredentials Creds;
	Creds.Type = TEXT("noeas_+_EIK_ECT_DEVICEID_ACCESS_TOKEN");
	Creds.Id = TEXT("Guest_000");
	Creds.Token = TEXT("");

	Identity->OnLoginCompleteDelegates->AddUObject(
		this,
		&UCSAuthSubsystem::OnLoginComplete
	);

	Identity->Login(0, Creds);
}

void UCSAuthSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if ( bWasSuccessful )
	{
		UE_LOG(LogCS, Log, TEXT("[UCSAuthSubsystem] DeviceID Login Success: %s"), *UserId.ToString());
	}
	else
	{
		UE_LOG(LogCS, Error, TEXT("[UCSAuthSubsystem] Login Failed: %s"), *Error);
	}
}
