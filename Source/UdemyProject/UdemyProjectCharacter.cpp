// Copyright Epic Games, Inc. All Rights Reserved.

#include "UdemyProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AUdemyProjectCharacter

AUdemyProjectCharacter::AUdemyProjectCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->GroundFriction = 500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	static ConstructorHelpers::FObjectFinder<UInputAction> DashInput = TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Dash.IA_Dash'");
	if (DashInput.Object) {
		DashAction = DashInput.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeInput = TEXT("/Script/EnhancedInput.InputAction'/Game/ThirdPerson/Input/Actions/IA_Dodge.IA_Dodge'");
	if (DodgeInput.Object) {
		DodgeAction = DodgeInput.Object;
	}

	const ConstructorHelpers::FObjectFinder<UCurveFloat> Curve(TEXT("/Script/Engine.CurveFloat'/Game/Udemy/Character/Dodge/CV_Dodge.CV_Dodge'"));
	if (Curve.Succeeded()) {
		DodgeCurve = Curve.Object;
	}

	DodgeTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DodgeTimeline"));
	DodgeDistance = 500.0f;
	DashDirection = FVector::ZeroVector;
	DashVelocity = FVector::ZeroVector;
}

void AUdemyProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	FOnTimelineFloat DodgeCallback;
	DodgeCallback.BindUFunction(this, FName("DodgeInterpReturn"));
	DodgeTimeline->SetLooping(false);
	DodgeTimeline->AddInterpFloat(DodgeCurve, DodgeCallback);
	DodgeTimeline->SetTimelineLength(0.25f);
}

void AUdemyProjectCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GetCharacterMovement()->MoveUpdatedComponent(FVector(1.0f, 1.0f, 0.0f), GetActorRotation(), true);
	GetCharacterMovement()->MoveUpdatedComponent(FVector(-1.0f, -1.0f, 0.0f), GetActorRotation(), true);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AUdemyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AUdemyProjectCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AUdemyProjectCharacter::Look);

		//Dashing
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AUdemyProjectCharacter::StartDash);
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Completed, this, &AUdemyProjectCharacter::StopDash);

		//Dodging
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Triggered, this, &AUdemyProjectCharacter::DodgeCheck);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AUdemyProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AUdemyProjectCharacter::StartDash(const FInputActionValue& Value)
{
	GetCharacterMovement()->MaxWalkSpeed = 800.0f;
}

void AUdemyProjectCharacter::StopDash(const FInputActionValue& Value)
{
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
}

void AUdemyProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AUdemyProjectCharacter::DodgeCheck(const FInputActionValue& Value)
{
	// If input vector not equal ZeroVector
	if (GetCharacterMovement()->GetLastInputVector() != FVector::ZeroVector) {
		FHitResult HitResult;

		bool IsHit = GetWorld()->LineTraceSingleByChannel(HitResult,
			GetActorLocation(),
			GetActorLocation() + (GetCharacterMovement()->GetLastInputVector() * DodgeDistance),
			ECollisionChannel::ECC_Visibility);

		if (IsHit)
			Dodge(HitResult.Location + (GetCharacterMovement()->GetLastInputVector() * -55.0f), GetActorForwardVector());
		else
			Dodge(GetActorLocation() + (GetCharacterMovement()->GetLastInputVector() * DodgeDistance), GetActorForwardVector());
	}
}

void AUdemyProjectCharacter::Dodge(const FVector DashDir, const FVector DashVel)
{
	DashDirection = DashDir;
	DashVelocity = DashVel;
	DodgeTimeline->PlayFromStart();
}

void AUdemyProjectCharacter::DodgeInterpReturn(float value)
{
	SetActorLocation(FMath::Lerp(GetActorLocation(), DashDirection, value));
}