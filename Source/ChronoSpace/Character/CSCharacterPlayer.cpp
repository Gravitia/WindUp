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
#include "ActorComponent/CSCharacterPushedComponent.h"
#include "ActorComponent/CSCharacterPulledByBlackhole.h"
#include "ActorComponent/CSCameraZoomComponent.h"
#include "ActorComponent/CSVFXComponent.h"
#include "Player/CSPlayerController.h"
#include "DataAsset/CSCharacterPlayerData.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Actor/CSBlackHole.h"
#include "Actor/CSBellows.h"
#include "Subsystem/CSManagedActorSubsystem.h"
#include "Kismet/GameplayStatics.h"


ACSCharacterPlayer::ACSCharacterPlayer()
{
	bReplicates = true;

	bIsFirstLook = false;

	// Camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	ZoomComponent = CreateDefaultSubobject<UCSCameraZoomComponent>(TEXT("ZoomComponent"));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	// 보통 플레이어 메쉬의 “head” 소켓(혹은 눈 위치)에 붙입니다.
	FirstPersonCamera->SetupAttachment(GetMesh(), TEXT("head"));
	FirstPersonCamera->bUsePawnControlRotation = true;
	// 처음엔 1인칭 카메라 비활성화
	FirstPersonCamera->SetActive(false);

	// ASC
	ASC = nullptr;

	// Trigger
	Trigger = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trigger"));
	Trigger->SetCollisionProfileName( CPROFILE_OVERLAPALL );
	Trigger->SetupAttachment(GetCapsuleComponent());

	// 캐占쏙옙占싶울옙 占쏙옙占쏙옙占쏙옙트 占쌩곤옙
	PushingCharacterComponent = CreateDefaultSubobject<UCSPushingCharacterComponent>(TEXT("PushingCharacterComponent"));
	PushingCharacterComponent->SetTrigger(Trigger);

	InteractionComponent = CreateDefaultSubobject<UCSPlayerInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetTrigger(Trigger);

	ScaleComponent = CreateDefaultSubobject<UCSCharacterScaleComponent>(TEXT("ScaleComponent"));
	
	GASManagerComponent = CreateDefaultSubobject<UCSGASManagerComponent>(TEXT("GASManagerComponent"));

	TransformRecordComponent = CreateDefaultSubobject<UCSTransformRecordComponent>(TEXT("TransformRecordComponent"));

	PushedComponent = CreateDefaultSubobject<UCSCharacterPushedComponent>(TEXT("PushedComponent"));

	PulledByBlackholeComponent = CreateDefaultSubobject<UCSCharacterPulledByBlackhole>(TEXT("PulledByBlackholeComponent"));

	GravityCoreSphere = CreateDefaultSubobject<USphereComponent>(TEXT("GravityCoreSphere"));
	GravityCoreSphere->SetIsReplicated(true);
	GravityCoreSphere->SetCollisionProfileName(CPROFILE_GRAVITY_CORE);
	GravityCoreSphere->SetCollisionObjectType(CCHANNEL_CSGRAVITY_CORE);
	GravityCoreSphere->SetupAttachment(RootComponent);
	GravityCoreSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//GravityCoreSphere->SetCollisionResponseToChannel(CCHANNEL_CSSPECTATOR, ECR_Ignore);
	GravityCoreSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	GravityCoreSphere->SetCollisionResponseToChannel(
		CCHANNEL_CSGRAVITY_CORE_AFFECTED,
		ECR_Block
	);
	GravityCoreSphere->SetCollisionResponseToChannel(
		CCHANNEL_CSPLAYER,
		ECR_Block
	);
	//GravityCoreSphere->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetCapsuleComponent()->SetCollisionObjectType(CCHANNEL_CSPLAYER);
	GetCapsuleComponent()->SetCollisionResponseToChannel(CCHANNEL_CSSPECTATOR, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(CCHANNEL_CSSPECTATOR, ECR_Ignore);
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
	GASManagerComponent->SetupGASInputComponent(Cast<UEnhancedInputComponent>(InputComponent));
}

void ACSCharacterPlayer::Landed(const FHitResult& Hit)
{
	// 풀무 로직 수정으로 인한 Landed 주석 처리 
	/*
	Super::Landed(Hit);

	// 밟은 Actor 가져오기
	AActor* LandedActor = Hit.GetActor();
	if (!LandedActor) return;

	// Bellows인지 검사
	ACSBellows* Bellows = Cast<ACSBellows>(LandedActor);
	if (Bellows)
	{
		Bellows->NotifyPlayerLanded(this);
	}
	*/
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

	/* AlwaysClockUnwind not using now 
	AlwaysClockUnwind();
	*/
}

void ACSCharacterPlayer::AlwaysClockUnwind()
{
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

void ACSCharacterPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACSCharacterPlayer, BlackHole);
}

void ACSCharacterPlayer::SetDead()
{
	Super::SetDead();

	APlayerController* PlayerController = Cast<APlayerController>(GetController()); 
	if (PlayerController) 
	{
		DisableInput(PlayerController); 
	}

	VFXComponent->PlayWorldVFX(EWorldVFX::EFFECT_DEAD_0);
}

void ACSCharacterPlayer::SetRevive()
{
	Super::SetRevive();

	// Sound 

	if (ReviveSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ReviveSound,
			GetActorLocation()
		);
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController)
	{
		EnableInput(PlayerController);
	}
}

void ACSCharacterPlayer::SetData()
{
	if (Data == nullptr) return;

	CameraBoom->TargetArmLength = Data->TargetArmLength;
	CameraBoom->SetRelativeLocation(Data->CameraOffset);

	if ( ZoomComponent )
	{
		ZoomComponent->Init(CameraBoom, Data->TargetArmLength);
	}

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

	// GetMesh()->SetSkeletalMesh(Data->Mesh);
	// GetMesh()->SetAnimInstanceClass(Data->AnimInstance);
	GetMesh()->SetRelativeLocation(Data->MeshLocation);
	GetMesh()->SetRelativeRotation(Data->MeshRotation); 

	Trigger->SetCapsuleSize(Data->TriggerRadius, Data->TriggerHeight); 
}

void ACSCharacterPlayer::ZoomCamera( float ZoomLength, float ZoomSpeed )
{
	if ( Data == nullptr || ZoomComponent == nullptr ) return;

	ZoomComponent->ZoomCamera(ZoomLength, ZoomSpeed);
}

void ACSCharacterPlayer::ShoulderMove(const FInputActionValue& Value)
{
	// 1) 입력 축 (X = Forward, Y = Right)
	const FVector2D MoveAxis = Value.Get<FVector2D>();
	if (MoveAxis.IsNearlyZero()) return;

	// 2) 컨트롤러의 Yaw만 추출
	const FRotator ControlRot = Controller ? Controller->GetControlRotation()
		: FRotator::ZeroRotator;
	const FRotator YawOnlyRot(0.f, ControlRot.Yaw, 0.f);

	// 3) 현재 중력 ↓ 구하기
	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	const FVector GravityDir = MoveComp
		? -MoveComp->GetGravityDirection() 
		: FVector(0, 0, -1);               

	FVector ForwardDir = FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::X);
	FVector RightDir = FRotationMatrix(YawOnlyRot).GetUnitAxis(EAxis::Y);

	ForwardDir = FVector::VectorPlaneProject(ForwardDir, GravityDir).GetSafeNormal();
	RightDir = FVector::VectorPlaneProject(RightDir, GravityDir).GetSafeNormal();

	RightDir = FVector::CrossProduct(GravityDir, ForwardDir).GetSafeNormal();

	AddMovementInput(ForwardDir, MoveAxis.X);
	AddMovementInput(RightDir, MoveAxis.Y);
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

float ACSCharacterPlayer::GetReviveTime()
{
	if (Data == nullptr) return 1.0f;
	return Data->ReviveDelay;
}

void ACSCharacterPlayer::ServerDestoryBlackHole_Implementation()
{
	if ( BlackHole )
	{
		BlackHole->SetDuration(0.2f);
	}
}

void ACSCharacterPlayer::NetMulticastMakeGravityCoreSphere_Implementation(float SphereRaduis, float SphereScale)
{
	if (GravityCoreSphere) 
	{
		UE_LOG(LogCS, Log, TEXT("GravityCore On"));
		GravityCoreSphere->SetSphereRadius(SphereRaduis * SphereScale);
		GravityCoreSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void ACSCharacterPlayer::NetMulticastDestroyGravityCoreSphere_Implementation()
{
	if (GravityCoreSphere)
	{
		UE_LOG(LogCS, Log, TEXT("GravityCore Off"));
		GravityCoreSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ACSCharacterPlayer::ServerSpawnAndSetBlackHole_Implementation(TSubclassOf<class ACSBlackHole> BlackHoleClass,
	FVector Direction, float MaxDistance, float Duration, float GravityInfluenceRange, float PullStrength, float StopRange, bool bCheckComponent)
{
	if (UWorld* World = GetWorld())
	{
		FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight); 
		FVector EndLocation = StartLocation + Direction * MaxDistance; 

		FCollisionQueryParams QueryParams; 
		QueryParams.AddIgnoredActor(this); 

		if ( IsValid(GetWorld()) )
		{
			if ( UCSManagedActorSubsystem* Subsystem = GetWorld()->GetSubsystem<UCSManagedActorSubsystem>(); Subsystem )
			{
				QueryParams.AddIgnoredActors( Subsystem->GetActorsPulledByBlackHole() );
			}
		}

		FHitResult HitResult; 
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams)) 
		{
			EndLocation = HitResult.Location; 
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;

		FRotator Rotation = FRotator::ZeroRotator;
		BlackHole = World->SpawnActor<ACSBlackHole>(BlackHoleClass, EndLocation, Rotation, Params);

		if (BlackHole)
		{
			BlackHole->SetDuration(Duration);
			BlackHole->SetGravityInfluenceRange(GravityInfluenceRange);
			BlackHole->SetPullStrength(PullStrength);
			BlackHole->SetStopRange(StopRange);
			BlackHole->SetCheckComponentInMesh(bCheckComponent);
		}
	}
}

void ACSCharacterPlayer::ServerSetBlackHoleLocation_Implementation(FVector Direction, float MaxDistance) 
{
	if (!IsValid(BlackHole)) return; 

	FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	FVector EndLocation = StartLocation + Direction * MaxDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor( this );

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		EndLocation = HitResult.Location;
	}

	BlackHole->SetActorLocation(EndLocation);
}