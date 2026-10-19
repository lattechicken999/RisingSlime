#include "SlimeShot.h"
#include "CreatureAgent.h"
#include "SlimePawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASlimeShot::ASlimeShot()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(RadiusCm);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.28f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Visual->SetStaticMesh(SphereMesh.Object);
	}
}

void ASlimeShot::Launch(ASlimePawn* InOwner, const FVector& Direction, float InDamage, bool bInPoison, float InPoisonDps, float InPoisonDuration)
{
	ShotOwner = InOwner;
	MoveDirection = Direction.SizeSquared() > KINDA_SMALL_NUMBER ? Direction.GetSafeNormal() : FVector::ForwardVector;
	Damage = InDamage;
	bPoison = bInPoison;
	PoisonDps = InPoisonDps;
	PoisonDuration = InPoisonDuration;
	SetActorRotation(MoveDirection.Rotation());
}

void ASlimeShot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = SpeedCm * DeltaSeconds;
	if (TryHit(Step))
	{
		return;
	}

	AddActorWorldOffset(MoveDirection * Step, false);
	Life -= DeltaSeconds;
	if (Life <= 0.f)
	{
		Destroy();
	}
}

bool ASlimeShot::TryHit(float StepCm)
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeShot), false, this);
	if (ShotOwner)
	{
		Params.AddIgnoredActor(ShotOwner);
	}
	Params.AddIgnoredActor(this);

	const FVector Start = GetActorLocation();
	const FVector End = Start + MoveDirection * StepCm;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(RadiusCm), Params);

	if (!bHit)
	{
		return false;
	}

	if (ACreatureAgent* Agent = Cast<ACreatureAgent>(Hit.GetActor()))
	{
		if (!Agent->IsDowned())
		{
			Agent->TakeHit(Damage, bPoison, PoisonDps, PoisonDuration);
		}
	}

	Destroy();
	return true;
}
