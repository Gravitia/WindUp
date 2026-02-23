// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/CSPlayerController.h"
#include "UI/CSGameUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/CSGameMode.h"
#include "Subsystem/CSSplitScreenSubsystem.h"
#include "Actor/CSCameraViewProxy.h"
#include "Pawn/CSSpectatorPawn.h"
#include "TimerManager.h"
#include "Character/CSCharacterPlayer.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "ChronoSpace.h"
#include "GameFramework/SpringArmComponent.h"


ACSPlayerController::ACSPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetupInputMode();

	// UI 생성을 약간 지연 (PlayerState 초기화 대기)
	// GetWorldTimerManager().SetTimer(UICreationTimerHandle, this, &ACSPlayerController::InitializeUI, 0.2f, false);
	/*
	if (!bIsDummyController)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Player Controller Started - IsLocalController: %s"),
			IsLocalController() ? TEXT("true") : TEXT("false"));

		// 클라이언트에서 로컬 컨트롤러인 경우 스플릿 스크린 설정
		if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
		{
			UE_LOG(LogTemp, Warning, TEXT("SS Client detected - Setting up split screen"));

			// 이미 설정이 완료되었는지 체크
			if (bClientSplitScreenSetupComplete)
			{
				UE_LOG(LogTemp, Warning, TEXT("SS Client split screen already setup, skipping"));
				return;
			}

			// GameInstance에서 스플릿 스크린 활성화
			if ( UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>() )
			{
				CSSplitSubsystem->EnableSplitScreen();
			}

			// 더미 로컬 플레이어 생성 (한 번만)
			FTimerHandle ClientSetupHandle;
			GetWorldTimerManager().SetTimer(
				ClientSetupHandle,
				[this]()
				{
					if (!bClientSplitScreenSetupComplete) // 다시 한번 체크
					{
						SetupClientSplitScreen();
					}
				},
				1.0f, // n초 지연
				false
			);
		}
	}
	*/
}

void ACSPlayerController::ShakeCamera()
{
	if (CameraShake)
	{
		ClientStartCameraShake(CameraShake);
	}
}

void ACSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetupInputMode();

	// Pawn이 변경될 때 UI 새로고침
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState가 복제될 때 UI 새로고침
	if (GameUIWidget)
	{
		RefreshGameUI();
	}
}

void ACSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	GetWorldTimerManager().ClearTimer(UICreationTimerHandle);
	GetWorldTimerManager().ClearTimer(ClientSyncTimerHandle);

	CleanupDummyLocalPlayer();
	
	// UI 정리
	if (GameUIWidget)
	{
		GameUIWidget->RemoveFromParent();
		GameUIWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ACSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	//InputComponent->BindAction("DualMode", IE_Pressed, this, &ACSPlayerController::ToggleDualMode);
	if (bIsDummyController)
	{
		UE_LOG(LogTemp, Log, TEXT("SS Dummy Controller - Skipping input setup"));
		return;
	}
}

void ACSPlayerController::SetupInputMode()
{
	// 게임 전용 입력 모드 설정
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void ACSPlayerController::InitializeUI()
{
	// 로컬 플레이어 컨트롤러에서만 UI 생성
	if (IsLocalPlayerController())
	{
		ShowGameUI();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				TEXT("Game UI Initialized for Local Player"));
		}
	}
}


void ACSPlayerController::CreateGameUI()
{
	if (GameUIWidgetClass && !GameUIWidget && IsLocalPlayerController())
	{
		GameUIWidget = CreateWidget<UCSGameUIWidget>(this, GameUIWidgetClass);

		if (GEngine && GameUIWidget)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue,
				TEXT("Game UI Widget Created"));
		}
	}
}

void ACSPlayerController::ShowGameUI()
{
	// 로컬 플레이어에서만 UI 표시
	if (!IsLocalPlayerController())
	{
		return;
	}

	CreateGameUI();

	if (GameUIWidget)
	{
		if (!GameUIWidget->IsInViewport())
		{
			GameUIWidget->AddToViewport();

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
					TEXT("Game UI Added to Viewport"));
			}
		}

		GameUIWidget->SetVisibility(ESlateVisibility::Visible);

		// UI 새로고침
		RefreshGameUI();
	}
}

void ACSPlayerController::HideGameUI()
{
	if (GameUIWidget)
	{
		GameUIWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ACSPlayerController::ToggleGameUI()
{
	if (IsGameUIVisible())
	{
		HideGameUI();
	}
	else
	{
		ShowGameUI();
	}
}

void ACSPlayerController::RefreshGameUI()
{
	if (GameUIWidget)
	{
		GameUIWidget->RefreshPlayerUI();
	}
}

bool ACSPlayerController::IsGameUIVisible() const
{
	return GameUIWidget &&
		GameUIWidget->IsInViewport() &&
		GameUIWidget->GetVisibility() == ESlateVisibility::Visible;
}

void ACSPlayerController::SetupClientSplitScreen()
{
	// 이미 설정 완료된 경우 리턴
	if (bClientSplitScreenSetupComplete)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Client split screen setup already complete"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SS Setting up client split screen"));

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	int32 CurrentLocalPlayers = GameInstance->GetNumLocalPlayers();
	UE_LOG(LogTemp, Warning, TEXT("SS Client current local players: %d"), CurrentLocalPlayers);

	if (CurrentLocalPlayers >= 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Client already has 2+ local players"));

		// 이미 LocalPlayer가 있다면 더미 폰만 생성 (한 번만)
		if (!ClientDummyPawn)
		{
			// CreateClientDummyPawn();
		}

		// 설정 완료 플래그 설정
		bClientSplitScreenSetupComplete = true;
		return;
	}

	// 더미 로컬 플레이어 생성
	FPlatformUserId DummyUserId = FPlatformUserId::CreateFromInternalId(1);
	FString OutError;
	ULocalPlayer* DummyLocalPlayer = GameInstance->CreateLocalPlayer(DummyUserId, OutError, false);

	if (DummyLocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Client dummy local player created successfully"));
		// LocalPlayer 생성 성공 후 더미 폰 생성
		CreateClientDummyPawn();

		// 설정 완료 플래그 설정
		bClientSplitScreenSetupComplete = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SS Client failed to create dummy local player: %s"), *OutError);
	}
}

void ACSPlayerController::CreateClientDummyPawn()
{
	UE_LOG(LogCS, Log, TEXT("[NetMode : %d] ACSPlayerController::CreateClientDummyPawn"), GetWorld()->GetNetMode());

	// 이미 더미 폰이 있다면 리턴
	if (ClientDummyPawn && IsValid(ClientDummyPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn already exists and is valid"));
		return;
	}

	FVector DummySpawnLocation = FVector(0, 0, 200);
	FRotator DummySpawnRotation = FRotator::ZeroRotator;

	ClientDummyPawn = GetWorld()->SpawnActor<ACSSpectatorPawn>(
		ACSSpectatorPawn::StaticClass(),
		DummySpawnLocation,
		DummySpawnRotation
	);

	if (ClientDummyPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("SS Client dummy pawn created successfully"));

		ClientDummyPawn->Rename(TEXT("ClientSpectatorPawn"));

		// *** 중요: 더미 컨트롤러를 새로 생성하지 않고 기존 것 활용 ***

		// 1) 먼저 기존 더미 컨트롤러 찾기
		ACSPlayerController* ExistingDummyController = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			ACSPlayerController* PC = Cast<ACSPlayerController>(*It);
			if (PC && PC->bIsDummyController && PC != this)
			{
				ExistingDummyController = PC;
				UE_LOG(LogTemp, Warning, TEXT("SS Found existing dummy controller: %s"), *PC->GetName());
				break;
			}
		}

		ACSPlayerController* DummyController = ExistingDummyController;

		// 2) 기존 더미 컨트롤러가 없을 때만 새로 생성
		if (!DummyController)
		{
			// 새 컨트롤러 생성 전에 정말 필요한지 다시 체크
			UGameInstance* GameInstance = GetGameInstance();
			if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
			{
				ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
				
				if (SecondLocalPlayer && !SecondLocalPlayer->PlayerController)
				{
					// 두 번째 LocalPlayer에 컨트롤러가 없을 때만 생성
					DummyController = GetWorld()->SpawnActor<ACSPlayerController>();
					if (DummyController)
					{
						DummyController->SetAsDummyController(true);
						UE_LOG(LogTemp, Warning, TEXT("SS New dummy controller created: %s"), *DummyController->GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("SS SecondLocalPlayer already has controller, skipping creation"));
					// 이미 컨트롤러가 있다면 그것을 사용
					if (SecondLocalPlayer->PlayerController)
					{
						if (ACSPlayerController* SSPC = Cast<ACSPlayerController>(SecondLocalPlayer->PlayerController))
						{
							DummyController = SSPC;
							DummyController->SetAsDummyController(true);
						}
					}
				}
			}
		}

		// 3) 컨트롤러 설정
		if (DummyController)
		{
			UGameInstance* GameInstance = GetGameInstance();
			if (GameInstance && GameInstance->GetNumLocalPlayers() >= 2)
			{
				ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);
				if (SecondLocalPlayer)
				{
					// 기존 연결이 없을 때만 설정
					if (!SecondLocalPlayer->PlayerController || SecondLocalPlayer->PlayerController != DummyController)
					{
						DummyController->SetPlayer(SecondLocalPlayer);
					}

					// 폰이 소유되지 않았을 때만 Possess
					if (!ClientDummyPawn->GetController() || ClientDummyPawn->GetController() != DummyController)
					{
						DummyController->Possess(ClientDummyPawn);
					}

					UE_LOG(LogTemp, Warning, TEXT("SS Client dummy controller setup complete"));
				}
			}
		}

		// 4) 클라이언트 동기화 시작 (한 번만)
		if (!GetWorldTimerManager().IsTimerActive(ClientSyncTimerHandle))
		{
			StartClientDummySync(ClientDummyPawn);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SS Failed to create client dummy pawn"));
	}

	AttachDummySpectatorToRemoteCharacter(ClientDummyPawn);
}


void ACSPlayerController::StartClientDummySync(ACSSpectatorPawn* DummyPawn)
{
	if (!DummyPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("SS Cannot start sync - dummy pawn is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SS Starting client dummy sync"));
	/*
	// 클라이언트에서 원격 플레이어와 동기화
	GetWorldTimerManager().SetTimer(
		ClientSyncTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, DummyPawn]()
			{
				if (!IsValid(this) || !IsValid(DummyPawn))
					return;
				if (GetWorld()->bIsTearingDown)
					return;

				SyncClientDummyWithRemotePlayer(DummyPawn);
			}),
		0.033f,
		true
	);
	*/
}

void ACSPlayerController::SyncClientDummyWithRemotePlayer(ACSSpectatorPawn* DummyPawn)
{
	if (!DummyPawn) return;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("World is null in SyncClientDummyWithRemotePlayer"));
		return;
	}

	// 1) 서버 프록시 찾기
	ACSCameraViewProxy* Proxy = CachedProxy.Get();
	if (!Proxy)
	{
		for (TActorIterator<ACSCameraViewProxy> It(GetWorld()); It; ++It)
		{
			if (!It->IsServerProxy()) continue;
			Proxy = *It;
			break;
		}
		if (!Proxy) return;
		CachedProxy = Proxy;
	}

	// 2) 최신 서버 카메라 데이터 가져오기
	const FRepCamInfo& ServerCam = Proxy->GetReplicatedCamera();

	// 3) 새 데이터 도착 시 바로 적용
	if (!LastServerCamera.Location.Equals(ServerCam.Location, 0.001f) ||
		!LastServerCamera.Rotation.Equals(ServerCam.Rotation, 0.001f))
	{
		// LastServerCamera = ServerCam; // 단순 복사도 가능

		FCameraPredictionData CamData;
		CamData.Location = ServerCam.Location;
		CamData.Rotation = ServerCam.Rotation;
		CamData.Velocity = FVector::ZeroVector;
		CamData.AngularVelocity = FVector::ZeroVector;
		CamData.FOV = ServerCam.FOV;
		CamData.Timestamp = GetWorld()->GetTimeSeconds();

		ApplyCamera(DummyPawn, CamData);
	}
}


void ACSPlayerController::ApplyCamera(ACSSpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData)
{
	if (!DummyPawn) return;

	UWorld* World = GetWorld();
	if (!World) return;

	ACSCharacterPlayer* RemoteChar = CachedRemoteCharacter.Get();

	if (!IsValid(RemoteChar) || RemoteChar->IsLocallyControlled())
	{
		CachedRemoteCharacter.Reset();
		RemoteChar = nullptr;

		for (TActorIterator<ACSCharacterPlayer> It(World); It; ++It)
		{
			ACSCharacterPlayer* Target = *It;
			if (!IsValid(Target)) continue;
			if (Target->IsLocallyControlled()) continue;

			CachedRemoteCharacter = Target;
			RemoteChar = Target;
			break;
		}

		if (!IsValid(RemoteChar))
			return;
	}

	const FVector TargetLoc = RemoteChar->GetActorLocation();
	const FVector CurrentLoc = DummyPawn->GetActorLocation();
	
	// 필요 없음. 
	//DummyPawn->SetActorLocation(FMath::VInterpTo(CurrentLoc, TargetLoc, World->GetDeltaSeconds(), 30.f));


	if (APlayerController* DummyController = Cast<APlayerController>(DummyPawn->GetController()))
	{
		const FRotator SmoothedRot =
			FMath::RInterpTo(DummyController->GetControlRotation(), CameraData.Rotation, World->GetDeltaSeconds(), 8.f);
		DummyController->SetControlRotation(SmoothedRot);
	}
	
}
void ACSPlayerController::SetAsDummyController(bool bDummy)
{
	bIsDummyController = bDummy;
	UE_LOG(LogTemp, Log, TEXT("SS Controller %s set as dummy: %s"),
		*GetName(), bDummy ? TEXT("Yes") : TEXT("No"));
}

void ACSPlayerController::CleanupDummyLocalPlayer()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("CleanupDummyLocalPlayer: GameInstance is null"));
		return;
	}

	// 로컬 플레이어가 2명 이상인 경우만 정리 시도
	if (GameInstance->GetNumLocalPlayers() > 1)
	{
		ULocalPlayer* SecondLocalPlayer = GameInstance->GetLocalPlayerByIndex(1);

		if (SecondLocalPlayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cleaning up dummy local player: %s"), *SecondLocalPlayer->GetName());

			// 1) 관련된 Pawn / Controller 정리
			if (APlayerController* DummyPC = SecondLocalPlayer->PlayerController)
			{
				if (DummyPC->GetPawn())
				{
					DummyPC->GetPawn()->Destroy();
				}
				DummyPC->Destroy();
			}

			// 2) 실제 LocalPlayer 제거
			GameInstance->RemoveLocalPlayer(SecondLocalPlayer);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CleanupDummyLocalPlayer: No dummy LocalPlayer found"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CleanupDummyLocalPlayer: Only one LocalPlayer exists, skipping"));
	}
}


void ACSPlayerController::AttachDummySpectatorToRemoteCharacter(ACSSpectatorPawn* DummyPawn)
{
	if (!DummyPawn)
	{
		UE_LOG(LogCS, Warning, TEXT("AttachDummySpectatorToRemoteCharacter: DummyPawn is null"));
		return;
	}

	// 1) 원격 캐릭터 찾기 (자신이 아닌 캐릭터)
	ACSCharacterPlayer* RemoteChar = nullptr;
	for (TActorIterator<ACSCharacterPlayer> It(GetWorld()); It; ++It)
	{
		ACSCharacterPlayer* Char = *It;
		if (!Char || Char->IsLocallyControlled()) continue;
		RemoteChar = Char;
		break;
	}

	if (!RemoteChar)
	{
		UE_LOG(LogCS, Warning, TEXT("AttachDummySpectatorToRemoteCharacter: No remote character found"));
		return;
	}

	// 2) 스켈레탈 메시 찾기
	USkeletalMeshComponent* Mesh = RemoteChar->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogCS, Warning, TEXT("AttachDummySpectatorToRemoteCharacter: RemoteChar has no mesh"));
		return;
	}

	// 3) 더미 스펙테이터를 캐릭터의 소켓(camera_socket 등)에 Attach
	FName AttachSocketName = TEXT("camera_socket");
	if (!Mesh->DoesSocketExist(AttachSocketName))
	{
		UE_LOG(LogCS, Warning, TEXT("AttachDummySpectatorToRemoteCharacter: Socket %s not found, using root"),
			*AttachSocketName.ToString());
		DummyPawn->AttachToActor(RemoteChar, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
		DummyPawn->AttachToComponent(Mesh, AttachRules, AttachSocketName);
	}

	DummyPawn->SetActorHiddenInGame(true);
	DummyPawn->SetActorEnableCollision(false);

	UE_LOG(LogCS, Warning, TEXT("Client: DummySpectator attached to %s"), *RemoteChar->GetName());
}


void ACSPlayerController::ServerBroadcastZoomToOthers_Implementation(float NewArmLength)
{
	// 서버 자신 (ListenServer)의 카메라도 즉시 변경
	if (UWorld* World = GetWorld())
	{
		if (ACSGameMode* GM = Cast<ACSGameMode>(World->GetAuthGameMode()))
		{
			if (GM->DummySpectatorPawn && GM->DummySpectatorPawn->CameraBoom)
			{
				GM->DummySpectatorPawn->CameraBoom->TargetArmLength = NewArmLength;
				UE_LOG(LogCS, Log, TEXT("Server: DummySpectator ArmLength set to %.1f"), NewArmLength);
			}
		}
	}

	// 다른 클라이언트들에게는 Client RPC로 전달
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ACSPlayerController* PC = Cast<ACSPlayerController>(*It);
		if (!PC || PC == this) continue; // 자기 자신 제외 (이미 위에서 처리)

		PC->ClientSetSpectatorCameraArmLength(NewArmLength);
	}
}

void ACSPlayerController::ClientSetSpectatorCameraArmLength_Implementation(float NewArmLength)
{
	if (ClientDummyPawn && ClientDummyPawn->CameraBoom)
	{
		ClientDummyPawn->CameraBoom->TargetArmLength = NewArmLength;

		UE_LOG(LogCS, Log, TEXT("ClientSetSpectatorCameraArmLength: %s's CameraBoom set to %.1f"),
			*GetName(), NewArmLength);
	}
}

void ACSPlayerController::Client_ApplyRespawnView_Implementation(const FRotator& Rot)
{
	SetControlRotation(Rot);

	// (선택) 몸도 같이 맞추고 싶으면
	if (APawn* P = GetPawn())
	{
		P->SetActorRotation(Rot);
	}
}

