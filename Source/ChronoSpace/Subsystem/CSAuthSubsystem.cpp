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
	if ( IdentityInterface.IsValid() )
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
	}
	
	Super::Deinitialize();
}

void UCSAuthSubsystem::LoginWithDeviceId()
{
	// for EOS
	IOnlineSubsystem* EOS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!EOS)
	{
		UE_LOG(LogCS, Warning, TEXT("No EOS Subsystem"));
		return;
	}
	//UE_LOG(LogCS, Log, TEXT("Valid EOS Subsystem"));

	IdentityInterface = EOS->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return;

	LoginCompleteHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(
		0,
		FOnLoginCompleteDelegate::CreateUObject(this, &UCSAuthSubsystem::OnLoginComplete)
	);

	// Deviceid login credentials setting
	FOnlineAccountCredentials Credenials;
	Credenials.Type = TEXT("deviceid");
	Credenials.Id = TEXT("");
	Credenials.Token = TEXT("");

	IdentityInterface->Login(0, Credenials);
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
