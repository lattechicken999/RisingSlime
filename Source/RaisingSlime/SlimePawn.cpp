#include "SlimePawn.h"
#include "ExpeditionDirector.h"
#include "Devourable.h"
#include "CreatureAgent.h"
#include "NestZone.h"
#include "SlimeShot.h"
#include "SlimeMovementComponent.h"
#include "JellyDeformerComponent.h"
#include "RaisingSlimePlayerController.h"
#include "RaisingSlimeHud.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

ASlimePawn::ASlimePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetupAttachment(SceneRoot);
	Collision->SetRelativeLocation(FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm));
	Collision->InitSphereRadius(57.5f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetSimulatePhysics(false);
	Collision->SetGenerateOverlapEvents(true);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
	}

	MainCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MainCamera"));
	MainCamera->SetupAttachment(SceneRoot);
	MainCamera->bUsePawnControlRotation = false;
	MainCamera->bOverrideAspectRatioAxisConstraint = true;
	MainCamera->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	MainCamera->AspectRatio = 1.777778f;
	MainCamera->bConstrainAspectRatio = false;
	MainCamera->FieldOfView = 110.43f;
	MainCamera->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));

	Movement = CreateDefaultSubobject<USlimeMovementComponent>(TEXT("Movement"));
	Jelly = CreateDefaultSubobject<UJellyDeformerComponent>(TEXT("Jelly"));
}

void ASlimePawn::BeginPlay()
{
	Super::BeginPlay();
	RefreshBody();
	ApplyBodySize();
	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController()))
	{
		PC->SetCursorUnlocked(false);
	}
}

void ASlimePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->RegisterSlime(this);
	}
}

void ASlimePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BindEnhancedInput();
}

void ASlimePawn::BindEnhancedInput()
{
	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController());
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!PC || !EIC)
	{
		return;
	}

	if (PC->GetMoveAction())
	{
		EIC->BindAction(PC->GetMoveAction(), ETriggerEvent::Triggered, this, &ASlimePawn::OnMove);
		EIC->BindAction(PC->GetMoveAction(), ETriggerEvent::Completed, this, &ASlimePawn::OnMove);
	}
	if (PC->GetLookAction())
	{
		EIC->BindAction(PC->GetLookAction(), ETriggerEvent::Triggered, this, &ASlimePawn::OnLook);
	}
	if (PC->GetJumpAction())
	{
		EIC->BindAction(PC->GetJumpAction(), ETriggerEvent::Started, this, &ASlimePawn::OnJumpStarted);
	}
	if (PC->GetAttackAction())
	{
		EIC->BindAction(PC->GetAttackAction(), ETriggerEvent::Started, this, &ASlimePawn::OnAttack);
	}
	if (PC->GetTraitAction())
	{
		EIC->BindAction(PC->GetTraitAction(), ETriggerEvent::Started, this, &ASlimePawn::OnTrait);
	}
	if (PC->GetCycleTraitAction())
	{
		EIC->BindAction(PC->GetCycleTraitAction(), ETriggerEvent::Started, this, &ASlimePawn::OnCycleTrait);
	}
	if (PC->GetDigestAction())
	{
		EIC->BindAction(PC->GetDigestAction(), ETriggerEvent::Started, this, &ASlimePawn::OnDigest);
	}
	if (PC->GetCursorAction())
	{
		EIC->BindAction(PC->GetCursorAction(), ETriggerEvent::Started, this, &ASlimePawn::OnCursor);
	}
	if (PC->GetFocusAction())
	{
		EIC->BindAction(PC->GetFocusAction(), ETriggerEvent::Started, this, &ASlimePawn::OnFocus);
	}
}

void ASlimePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController());
	const bool bMenu = IsMenuBlocking();

	// Raw mouse look when locked (bypass Mouse2D AxisConfig 0.07 so we can apply 0.22 deg/px)
	if (PC && !bMenu && !PC->IsCursorUnlocked())
	{
		float DeltaX = 0.f;
		float DeltaY = 0.f;
		PC->GetInputMouseDelta(DeltaX, DeltaY);
		if (!FMath::IsNearlyZero(DeltaX) || !FMath::IsNearlyZero(DeltaY))
		{
			ApplyLookFromRawPixels(FVector2D(DeltaX, DeltaY));
		}
	}

	if (bMenu)
	{
		MoveInput = FVector2D::ZeroVector;
		Channel = 0.f;
		Channel01 = 0.f;
		EngulfTarget = nullptr;
		Aimed = nullptr;
		bJumpQueued = false;
	}
	else
	{
		UpdateAim();
		Devour(DeltaSeconds);

		ANestZone* Nest = Cast<ANestZone>(UGameplayStatics::GetActorOfClass(GetWorld(), ANestZone::StaticClass()));
		ARaisingSlimeHUD* Hud = Cast<ARaisingSlimeHUD>(PC ? PC->GetHUD() : nullptr);
		if (Hud && Hud->IsDigestMenuOpen() && Nest && !Nest->IsPlayerInside())
		{
			Hud->CloseDigestMenu();
		}
	}

	FixedAccumulator += DeltaSeconds;
	int32 Steps = 0;
	AExpeditionDirector* Director = FindDirector();
	while (FixedAccumulator >= RaisingSlimeUnits::FixedStep && Steps < RaisingSlimeUnits::MaxSubsteps)
	{
		bool bConsumed = false;
		Movement->StepFixed(RaisingSlimeUnits::FixedStep, this, Director, MoveInput, bMenu, bJumpQueued, bConsumed);
		if (bConsumed)
		{
			bJumpQueued = false;
		}
		Jelly->Step(RaisingSlimeUnits::FixedStep);
		FixedAccumulator -= RaisingSlimeUnits::FixedStep;
		++Steps;
	}

	ApplyBodySize();
	SetActorRotation(FRotator(0.f, Yaw, 0.f));
	MainCamera->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void ASlimePawn::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
}

void ASlimePawn::OnLook(const FInputActionValue& /*Value*/)
{
	// Look is applied from raw cursor delta in Tick to avoid AxisConfig 0.07 scale.
}

void ASlimePawn::ApplyLookFromRawPixels(const FVector2D& PixelDelta)
{
	Yaw += PixelDelta.X * RaisingSlimeUnits::LookDegreesPerPixel;
	Pitch += PixelDelta.Y * RaisingSlimeUnits::LookDegreesPerPixel;
	Pitch = FMath::Clamp(Pitch, RaisingSlimeUnits::PitchMin, RaisingSlimeUnits::PitchMax);
}

void ASlimePawn::OnJumpStarted(const FInputActionValue& /*Value*/)
{
	if (!IsMenuBlocking())
	{
		bJumpQueued = true;
	}
}

void ASlimePawn::OnAttack(const FInputActionValue& /*Value*/)
{
	if (IsMenuBlocking())
	{
		return;
	}
	Attack();
}

void ASlimePawn::OnTrait(const FInputActionValue& /*Value*/)
{
	if (IsMenuBlocking())
	{
		return;
	}
	UseTrait();
}

void ASlimePawn::OnCycleTrait(const FInputActionValue& /*Value*/)
{
	if (IsMenuBlocking())
	{
		return;
	}
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->CycleActive();
	}
}

void ASlimePawn::OnDigest(const FInputActionValue& /*Value*/)
{
	ANestZone* Nest = Cast<ANestZone>(UGameplayStatics::GetActorOfClass(GetWorld(), ANestZone::StaticClass()));
	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController());
	ARaisingSlimeHUD* Hud = Cast<ARaisingSlimeHUD>(PC ? PC->GetHUD() : nullptr);
	if (Nest && Nest->IsPlayerInside() && Hud)
	{
		if (Hud->IsDigestMenuOpen())
		{
			Hud->CloseDigestMenu();
		}
		else
		{
			Hud->OpenDigestMenu();
		}
	}
}

void ASlimePawn::OnCursor(const FInputActionValue& /*Value*/)
{
	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController());
	ARaisingSlimeHUD* Hud = Cast<ARaisingSlimeHUD>(PC ? PC->GetHUD() : nullptr);
	if (Hud && Hud->IsDigestMenuOpen())
	{
		Hud->CloseDigestMenu();
		return;
	}
	if (PC)
	{
		PC->SetCursorUnlocked(true);
	}
}

void ASlimePawn::OnFocus(const FInputActionValue& /*Value*/)
{
	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController());
	if (PC && PC->IsCursorUnlocked() && !PC->IsMenuOpen())
	{
		PC->SetCursorUnlocked(false);
	}
}

void ASlimePawn::Attack()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackTime || !MainCamera)
	{
		return;
	}
	NextAttackTime = Now + 0.42f;

	AExpeditionDirector* Director = FindDirector();
	const FVector CamLoc = MainCamera->GetComponentLocation();
	const FVector CamFwd = MainCamera->GetForwardVector();
	FVector AimPoint = CamLoc + CamFwd * 3000.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAim), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, AimPoint, ECC_Visibility, Params))
	{
		AimPoint = Hit.ImpactPoint;
	}

	FVector Origin = BodyMesh->GetComponentLocation();
	FVector Direction = (AimPoint - Origin).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = CamFwd;
	}
	Origin += Direction * (GetBodySize() * 0.7f * RaisingSlimeUnits::MetersToCm);

	const bool bPoison = Director && Director->GetActiveTrait() == ETraitId::Poison;
	const bool bStable = bPoison && Director->IsActiveConfirmed();
	const float Damage = bStable ? 18.f : 14.f;
	const float Dps = bStable ? 8.f : 4.f;
	const float Dur = bStable ? 3.2f : 2.2f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	if (ASlimeShot* Shot = GetWorld()->SpawnActor<ASlimeShot>(Origin, Direction.Rotation(), SpawnParams))
	{
		Shot->Launch(this, Direction, Damage, bPoison, bPoison ? Dps : 0.f, bPoison ? Dur : 0.f);
	}
}

void ASlimePawn::UseTrait()
{
	AExpeditionDirector* Director = FindDirector();
	if (!Director)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Director->GetActiveTrait() == ETraitId::Harden)
	{
		if (Now < HardenCooldown)
		{
			return;
		}
		const bool bStable = Director->IsActiveConfirmed();
		HardenUntil = Now + (bStable ? 6.f : 3.5f);
		HardenCooldown = Now + 7.f;
	}
	else if (Director->GetActiveTrait() == ETraitId::Bounce)
	{
		Movement->AddBounceBurst(this, Director);
	}
}

void ASlimePawn::UpdateAim()
{
	Aimed = nullptr;
	if (!MainCamera)
	{
		return;
	}

	const FVector Start = MainCamera->GetComponentLocation();
	const FVector End = Start + MainCamera->GetForwardVector() * (ReachMeters() * RaisingSlimeUnits::MetersToCm);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeReach), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		Aimed = Cast<ADevourable>(Hit.GetActor());
	}
}

void ASlimePawn::Devour(float DeltaSeconds)
{
	AExpeditionDirector* Director = FindDirector();
	EngulfTarget = FindEngulfTarget();
	if (!Director || !EngulfTarget)
	{
		Channel = 0.f;
		Channel01 = 0.f;
		return;
	}

	Channel += DeltaSeconds;
	Channel01 = FMath::Clamp(Channel / EngulfTarget->GetChannelTime(), 0.f, 1.f);
	if (Channel >= EngulfTarget->GetChannelTime())
	{
		Channel = 0.f;
		Channel01 = 0.f;
		Director->CompleteDevour(EngulfTarget);
		EngulfTarget = nullptr;
	}
}

ADevourable* ASlimePawn::FindEngulfTarget() const
{
	const FVector Center = BodyMesh->GetComponentLocation();
	const float Radius = GetBodySize() * 0.78f * RaisingSlimeUnits::MetersToCm;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeDevour), false, this);
	GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);

	ADevourable* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADevourable* Prey = Cast<ADevourable>(Overlap.GetActor());
		if (!Prey || !Prey->CanDevour())
		{
			continue;
		}
		const float Dist = FVector::Dist(Center, Prey->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Prey;
		}
	}
	return Best;
}

void ASlimePawn::RefreshBody()
{
	AExpeditionDirector* Director = FindDirector();
	const float Mass = Director ? Director->GetTotalMass() : 0.f;
	const float NewMax = 100.f + Mass * 5.f;
	if (NewMax > MaxHp)
	{
		Hp += NewMax - MaxHp;
	}
	MaxHp = NewMax;
	Hp = FMath::Clamp(Hp, 0.f, MaxHp);
	ApplyBodySize();
}

void ASlimePawn::NotifyAte()
{
	AddImpact(0.16f);
}

void ASlimePawn::AddImpact(float Amount)
{
	if (Jelly && Amount > 0.f)
	{
		Jelly->AddImpulse(FVector(0.f, 0.f, -12.f), FVector(0.f, 0.f, 1.f), Amount * 6.f, 55.f, 1.1f, 1.15f);
	}
}

void ASlimePawn::Recover()
{
	RefreshBody();
	Hp = MaxHp;
	Movement->ResetMotion();
	bJumpQueued = false;
	Channel = 0.f;
	Channel01 = 0.f;
	HardenUntil = 0.f;
	Jelly->ResetMotion();
}

void ASlimePawn::TeleportToNest(const FTransform& NestTransform)
{
	SetActorTransform(NestTransform);
	Yaw = NestTransform.Rotator().Yaw;
	Pitch = RaisingSlimeUnits::DefaultPitch;
	Movement->ResetMotion();
	Jelly->ResetMotion();
	if (ANestZone* Nest = Cast<ANestZone>(UGameplayStatics::GetActorOfClass(GetWorld(), ANestZone::StaticClass())))
	{
		Nest->Refresh();
	}
}

void ASlimePawn::TakeDamageAmount(float Amount)
{
	if (Hp <= 0.f)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < HardenUntil)
	{
		AExpeditionDirector* Director = FindDirector();
		const bool bStable = Director && Director->IsActiveConfirmed();
		Amount *= bStable ? 0.3f : 0.5f;
	}

	Hp -= Amount;
	AddImpact(FMath::Clamp(Amount * 0.012f, 0.08f, 0.42f));
	Channel = 0.f;
	Channel01 = 0.f;
	if (Hp <= 0.f)
	{
		Hp = 0.f;
		if (AExpeditionDirector* Director = FindDirector())
		{
			Director->Die();
		}
	}
}

float ASlimePawn::GetBodySize() const
{
	AExpeditionDirector* Director = FindDirector();
	const float Mass = Director ? Director->GetTotalMass() : 0.f;
	return FMath::Clamp(1.15f + Mass * 0.05f, 1.15f, 3.2f);
}

float ASlimePawn::ReachMeters() const
{
	AExpeditionDirector* Director = FindDirector();
	const float Mass = Director ? Director->GetTotalMass() : 0.f;
	return 2.15f + Mass * 0.03f;
}

void ASlimePawn::ApplyBodySize()
{
	const float Size = GetBodySize();
	Collision->SetSphereRadius(Size * 50.f);
	BodyMesh->SetRelativeScale3D(FVector(Size));
	MainCamera->SetRelativeLocation(FVector(-Size * 255.f, 0.f, Size * 240.f));
}

AExpeditionDirector* ASlimePawn::FindDirector() const
{
	return Cast<AExpeditionDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AExpeditionDirector::StaticClass()));
}

bool ASlimePawn::IsMenuBlocking() const
{
	if (const ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetController()))
	{
		return PC->IsMenuOpen();
	}
	return false;
}

AActor* ASlimePawn::GetEngulfTarget() const
{
	return EngulfTarget;
}

AActor* ASlimePawn::GetAimedPrey() const
{
	return Aimed;
}
