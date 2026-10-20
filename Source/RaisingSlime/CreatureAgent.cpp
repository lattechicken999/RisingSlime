#include "CreatureAgent.h"
#include "SlimePawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RaisingSlimeTypes.h"

ACreatureAgent::ACreatureAgent()
{
	PrimaryActorTick.bCanEverTick = true;
	Kind = EPreyKind::Creature;
	Mass = 8.f;
	Trait = ETraitId::Poison;
	BaseChance = 0.18f;
	DisplayName = TEXT("벌");

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(40.f, 100.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetSimulatePhysics(false);

	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(Capsule);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ACreatureAgent::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
	Hp = MaxHp;
	Player = Cast<ASlimePawn>(UGameplayStatics::GetActorOfClass(GetWorld(), ASlimePawn::StaticClass()));

	if (MeshComponent)
	{
		DynMat = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ACreatureAgent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDowned || !Player)
	{
		return;
	}

	TickPoison(DeltaSeconds);
	if (bDowned)
	{
		return;
	}

	const float DistXY = FVector::DistXY(Player->GetActorLocation(), GetActorLocation()) / RaisingSlimeUnits::MetersToCm;
	const float Now = GetWorld()->GetTimeSeconds();

	FVector Destination;
	if (DistXY <= NoticeRange)
	{
		Destination = Player->GetActorLocation();
	}
	else
	{
		const FVector HomeOffset(
			FMath::Sin(Now * 0.45f) * 220.f,
			FMath::Cos(Now * 0.35f) * 220.f,
			0.f);
		Destination = HomeLocation + HomeOffset;
	}

	MoveTowards(Destination, DeltaSeconds);

	if (DistXY <= AttackRange && Now >= NextAttack)
	{
		NextAttack = Now + AttackInterval;
		Player->TakeDamageAmount(AttackDamage);
	}
}

void ACreatureAgent::TakeHit(float Damage, bool bApplyPoison, float Dps, float Duration)
{
	if (bDowned)
	{
		return;
	}

	Hp -= Damage;
	if (bApplyPoison)
	{
		PoisonDps = Dps;
		PoisonUntil = GetWorld()->GetTimeSeconds() + Duration;
		ApplyTint(FLinearColor(0.55f, 0.9f, 0.2f));
	}

	if (Hp <= 0.f)
	{
		KnockDown();
	}
}

void ACreatureAgent::Revive()
{
	Hp = MaxHp;
	bDowned = false;
	PoisonDps = 0.f;
	PoisonUntil = 0.f;
	NextAttack = 0.f;
	FallVelocity = -200.f;
	ClearTint();
}

void ACreatureAgent::RestoreScenePlacement()
{
	Super::RestoreScenePlacement();
	HomeLocation = GetActorLocation();
	Revive();
}

void ACreatureAgent::TickPoison(float DeltaSeconds)
{
	if (GetWorld()->GetTimeSeconds() >= PoisonUntil || PoisonDps <= 0.f)
	{
		return;
	}

	Hp -= PoisonDps * DeltaSeconds;
	if (Hp <= 0.f)
	{
		KnockDown();
	}
}

void ACreatureAgent::KnockDown()
{
	Hp = 0.f;
	bDowned = true;
	PoisonDps = 0.f;
	ApplyTint(FLinearColor(0.35f, 0.32f, 0.18f));
}

void ACreatureAgent::MoveTowards(const FVector& Destination, float DeltaSeconds)
{
	FVector Delta = Destination - GetActorLocation();
	Delta.Z = 0.f;
	FVector Step = FVector::ZeroVector;
	if (Delta.SizeSquared() > FMath::Square(20.f))
	{
		Step = Delta.GetSafeNormal() * (MoveSpeed * RaisingSlimeUnits::MetersToCm);
		const FQuat Target = FRotationMatrix::MakeFromX(Delta.GetSafeNormal()).ToQuat();
		const FQuat Current = GetActorQuat();
		const float Alpha = FMath::Clamp(8.f * DeltaSeconds, 0.f, 1.f);
		SetActorRotation(FQuat::Slerp(Current, Target, Alpha));
	}

	const FVector Location = GetActorLocation();
	const bool bGrounded = Location.Z <= HomeLocation.Z + 5.f;
	if (bGrounded && FallVelocity < 0.f)
	{
		FallVelocity = -200.f;
	}
	FallVelocity += RaisingSlimeUnits::CreatureGravityCm * DeltaSeconds;

	FVector NewLoc = Location + (Step + FVector(0.f, 0.f, FallVelocity)) * DeltaSeconds;
	if (NewLoc.Z < HomeLocation.Z)
	{
		NewLoc.Z = HomeLocation.Z;
		FallVelocity = -200.f;
	}
	SetActorLocation(NewLoc, true);
}

void ACreatureAgent::ApplyTint(FLinearColor Color)
{
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ACreatureAgent::ClearTint()
{
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.85f, 0.15f));
	}
}
