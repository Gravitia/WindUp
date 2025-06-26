// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CSCharacterPlayer.h"
#include "AbilitySystemComponent.h"
#include "Player/CSPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"
#include "UI/CSGASWidgetComponent.h"
#include "Character/CSF_CharacterFrameData.h"
#include "UI/CSGASUserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Actor/CSWhiteHall.h"
#include "Physics/CSCollision.h"
#include "Actor/CSGravityCore.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChronoSpace.h"
#include "ActorComponent/CSPlayerInteractionComponent.h"
#include "ActorComponent/CSPushingCharacterComponent.h"
#include "ActorComponent/CSCharacterScaleComponent.h"
#include "ActorComponent/CSGASManagerComponent.h"
#include "ActorComponent/CSTransformRecordComponent.h"
#include "Player/CSPlayerController.h"
#include "DataAsset/CSCharacterPlayerData.h"


ACSCharacterPlayer::ACSCharacterPlayer()
{
	bReplicates = true;

	// Camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// ASC
	ASC = nullptr;

	// Trigger
	Trigger = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trigger"));
	Trigger->SetCollisionProfileName( CPROFILE_OVERLAPALL );
	Trigger->SetupAttachment(GetCapsuleComponent());

	// 캐릭터에 컴포넌트 추가
	PushingCharacterComponent = CreateDefaultSubobject<UCSPushingCharacterComponent>(TEXT("PushingCharacterComponent"));
	PushingCharacterComponent->SetTrigger(Trigger);

	InteractionComponent = CreateDefaultSubobject<UCSPlayerInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetTrigger(Trigger);

	ScaleComponent = CreateDefaultSubobject<UCSCharacterScaleComponent>(TEXT("ScaleComponent"));
	
	GASManagerComponent = CreateDefaultSubobject<UCSGASManagerComponent>(TEXT("GASManagerComponent"));

	TransformRecordComponent = CreateDefaultSubobject<UCSTransformRecordComponent>(TEXT("TransformRecordComponent"));
}

UAbilitySystemComponent* ACSCharacterPlayer::GetAbilitySystemComponent() const
{
	return ASC;
}

void ACSCharacterPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	ACSPlayerState* CSPS = GetPlayerState<ACSPlayerState>();
	ASC = CSPS->GetAbilitySystemComponent(); 
	GASManagerComponent->SetASC(CSPS->GetAbilitySystemComponent(), CSPS);
	GASManagerComponent->SetGASAbilities();
}

void ACSCharacterPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	//UE_LOG(LogCS, Log, TEXT("[NetMode %d] SetupPlayerInputComponent"), GetWorld()->GetNetMode());
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInputComponent->BindAction(ShoulderMoveAction, ETriggerEvent::Triggered, this, &ACSCharacterPlayer::ShoulderMove);
	EnhancedInputComponent->BindAction(ShoulderLookAction, ETriggerEvent::Triggered, this, &ACSCharacterPlayer::ShoulderLook);


	GASManagerComponent->SetupGASInputComponent(Cast<UEnhancedInputComponent>(PlayerInputComponent));
	InteractionComponent->SetInteractionInputComponent(Cast<UEnhancedInputComponent>(PlayerInputComponent));
}

void ACSCharacterPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	ACSPlayerState* CSPS = GetPlayerState<ACSPlayerState>();
	ASC = CSPS->GetAbilitySystemComponent();
	GASManagerComponent->SetASC(ASC, CSPS);
}

void ACSCharacterPlayer::BeginPlay()
{
	Super::BeginPlay();
	//UE_LOG(LogCS, Log, TEXT("[NetMode: %d] BeginPlay"), GetWorld()->GetNetMode());

	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PlayerController = CastChecked<APlayerController>(GetController()); 
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))			
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}

	AttachWindUpKeyToSocket();
	
	if (HasAuthority())
	{
		// 서버에서 Multicast로 모든 클라이언트에 적용
		Multicast_ApplyClockUnwind();
	}
	else
	{
		// 클라이언트에서 서버에 요청
		Server_ApplyClockUnwind();
	}
}

void ACSCharacterPlayer::Server_ApplyClockUnwind_Implementation()
{
	// 서버에서 Multicast 호출
	Multicast_ApplyClockUnwind();
}

void ACSCharacterPlayer::Multicast_ApplyClockUnwind_Implementation()
{
	ApplyClockUnwind_Internal();
}

// 실제 GE 적용 함수 (변경 없음)
void ACSCharacterPlayer::ApplyClockUnwind_Internal()
{
	if (ClockUnwindEffect && ASC)
	{
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ClockUnwindEffect, 1.f, ASC->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void ACSCharacterPlayer::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	SetData();
}

void ACSCharacterPlayer::SetDead()
{
	Super::SetDead();

	APlayerController* PlayerController = Cast<APlayerController>(GetController()); 
	if (PlayerController) 
	{
		DisableInput(PlayerController); 
	}
}

void ACSCharacterPlayer::SetData()
{
	if (Data == nullptr) return;

	CameraBoom->TargetArmLength = Data->TargetArmLength;
	CameraBoom->SetRelativeLocation(Data->CameraOffset);

	GetCharacterMovement()->RotationRate = Data->RotationRate;
	GetCharacterMovement()->JumpZVelocity = Data->JumpZVelocity;
	GetCharacterMovement()->AirControl = Data->AirControl;
	GetCharacterMovement()->MaxWalkSpeed = Data->MaxWalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = Data->MinAnalogWalkSpeed;
	GetCharacterMovement()->BrakingDecelerationWalking = Data->BrakingDecelerationWalking;
	GetCharacterMovement()->GravityScale = Data->GravityScale;
	
	WalkSpeed = Data->MaxWalkSpeed;
	DashSpeed = Data->MaxDashSpeed;

	BaseCapsuleRadius = Data->CapsuleRadius;
	BaseCapsuleHalfHeight = Data->CapsuleHeight;

	GravityScale = Data->GravityScale;
	CoyoteTime = Data->CoyoteTime;

	GetCapsuleComponent()->SetCapsuleSize(Data->CapsuleRadius, Data->CapsuleHeight); 

	// SetCapsulSize vs InitCapsuleSize 

	GetMesh()->SetSkeletalMesh(Data->Mesh);
	GetMesh()->SetAnimInstanceClass(Data->AnimInstance);
	GetMesh()->SetRelativeLocation(Data->MeshLocation);
	GetMesh()->SetRelativeRotation(Data->MeshRotation); 

	Trigger->SetCapsuleSize(Data->TriggerRadius, Data->TriggerHeight); 
}

void ACSCharacterPlayer::ShoulderMove(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	//UE_LOG(LogCS, Log, TEXT("[NetMode %d] ShoulderMove"), GetWorld()->GetNetMode());
	AddMovementInput( FollowCamera->GetForwardVector(), MovementVector.X);
	AddMovementInput( FollowCamera->GetRightVector(), MovementVector.Y);
}

void ACSCharacterPlayer::ShoulderLook(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ACSCharacterPlayer::ClearWhiteHall()
{
	if ( IsValid(WhiteHall) )
	{
		WhiteHall->Destroy();
	}
	WhiteHall = nullptr;
}


void ACSCharacterPlayer::RequestUIRefresh()
{
	if (ACSPlayerController* PC = Cast<ACSPlayerController>(GetController()))
	{
		PC->RefreshGameUI();
	}
}


// 1. 점프 가능 조건에 코요테 bool OR 연산
bool ACSCharacterPlayer::CanJumpInternal_Implementation() const
{
	return Super::CanJumpInternal_Implementation() || bCanCoyoteJump;
}

// 2. 타이머 시작 / 종료
void ACSCharacterPlayer::StartCoyoteTimer()
{
	bCanCoyoteJump = true;
	GetWorldTimerManager().SetTimer(
		CoyoteTimerHandle,
		this,
		&ACSCharacterPlayer::DisableCoyoteTime,
		CoyoteTime,
		false);
}

void ACSCharacterPlayer::DisableCoyoteTime()
{
	bCanCoyoteJump = false;
}

// 3. 공중으로 들어가면 타이머 시작
void ACSCharacterPlayer::Falling()
{
	Super::Falling();
	StartCoyoteTimer();
}

// 4. 실제 점프가 발생하거나 착지하면 bool 리셋
void ACSCharacterPlayer::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	bCanCoyoteJump = false;
}

void ACSCharacterPlayer::OnMovementModeChanged(
	EMovementMode PrevMode, uint8 PrevCustomMode)
{
	Super::OnMovementModeChanged(PrevMode, PrevCustomMode);

	if (!bPressedJump && !GetCharacterMovement()->IsFalling())
	{
		bCanCoyoteJump = false;    // 착지 시 안전하게 종료
	}
}