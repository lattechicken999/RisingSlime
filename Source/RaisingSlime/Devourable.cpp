#include "Devourable.h"
#include "CombatTarget.h"
#include "CreatureAgent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADevourable::ADevourable()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MeshComponent->SetSimulatePhysics(false);
}

void ADevourable::BeginPlay()
{
	Super::BeginPlay();
	StoreHome();
}

void ADevourable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
		World->GetTimerManager().ClearTimer(SpitArmTimer);
	}
	Super::EndPlay(EndPlayReason);
}

FString ADevourable::GetLabel() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return TraitText::Name(Trait);
}

bool ADevourable::PassesDevourAvailability() const
{
	if (bConsumed || bInsideGut || IsHidden())
	{
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() < SpitGraceUntilTime)
		{
			return false;
		}
	}
	return true;
}

bool ADevourable::CanBeDevoured() const
{
	if (!PassesDevourAvailability())
	{
		return false;
	}

	// Non-hostile field prey (grass/rock/rubber/object): always.
	// Hostile combatants: subclass (CreatureAgent) requires Downed.
	if (PreyCombat::IsHostileCombatant(Kind))
	{
		return false;
	}
	return true;
}

void ADevourable::StoreHome()
{
	HomeTransform = GetActorTransform();
	HomeScale3D = GetActorScale3D();
	bHomeStored = true;
}

void ADevourable::DisableCollisionKeepVisible()
{
	bConsumed = true;
	DisarmSpitProjectile();
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	SetActorHiddenInGame(false);
	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetVisibility(true);
		MeshComponent->SetHiddenInGame(false);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ADevourable::BeginHeld()
{
	HeldWorldLocation = GetActorLocation();
	DisableCollisionKeepVisible();
	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	SetActorLocation(HeldWorldLocation);
	SetActorScale3D(HomeScale3D);
}

void ADevourable::SealIntoGut(USceneComponent* GutRoot, const FVector& LocalOffset)
{
	if (!GutRoot)
	{
		return;
	}

	bInsideGut = true;
	DisableCollisionKeepVisible();
	const FVector WorldScale = GetActorScale3D();
	AttachToComponent(GutRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(LocalOffset);
	SetActorRelativeScale3D(WorldScale);
	SetActorRelativeRotation(FRotator(
		FMath::FRandRange(-8.f, 8.f),
		FMath::FRandRange(0.f, 360.f),
		FMath::FRandRange(-6.f, 6.f)));
}

void ADevourable::CollectEngulfSamplePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	FVector Origin = GetActorLocation();
	FVector Extent = FVector(20.f);
	if (MeshComponent)
	{
		const FBoxSphereBounds B = MeshComponent->Bounds;
		Origin = B.Origin;
		Extent = B.BoxExtent;
	}
	OutPoints.Add(Origin);
	const FVector E = Extent * 0.85f;
	for (int32 SX = -1; SX <= 1; SX += 2)
	{
		for (int32 SY = -1; SY <= 1; SY += 2)
		{
			for (int32 SZ = -1; SZ <= 1; SZ += 2)
			{
				OutPoints.Add(Origin + FVector(SX * E.X, SY * E.Y, SZ * E.Z));
			}
		}
	}
}

void ADevourable::FinishGutAndRespawn()
{
	bInsideGut = false;
	DisarmSpitProjectile();
	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
		MeshComponent->SetHiddenInGame(true);
		MeshComponent->SetRelativeScale3D(FVector::OneVector);
	}
	SetActorScale3D(HomeScale3D);
	BeginRespawnCooldown();
}

void ADevourable::ArmSpitProjectile(AActor* InstigatorActor)
{
	DisarmSpitProjectile();

	SpitInstigator = InstigatorActor;
	bSpitProjectileArmed = true;
	bSpitDamageDealt = false;

	if (MeshComponent)
	{
		MeshComponent->SetNotifyRigidBodyCollision(true);
		MeshComponent->SetGenerateOverlapEvents(true);
		// Block Pawn so physics Hit fires against CreatureAgent capsules.
		MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		MeshComponent->OnComponentHit.AddUniqueDynamic(this, &ADevourable::OnSpitMeshHit);
		MeshComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADevourable::OnSpitMeshBeginOverlap);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpitArmTimer,
			this,
			&ADevourable::DisarmSpitProjectile,
			FMath::Max(0.25f, SpitReDevourGraceSeconds),
			false);
	}
}

void ADevourable::DisarmSpitProjectile()
{
	bSpitProjectileArmed = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpitArmTimer);
	}
	if (MeshComponent)
	{
		MeshComponent->OnComponentHit.RemoveAll(this);
		MeshComponent->OnComponentBeginOverlap.RemoveAll(this);
		MeshComponent->SetNotifyRigidBodyCollision(false);
		MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void ADevourable::TryDealSpitDamage(AActor* OtherActor)
{
	if (!bSpitProjectileArmed || bSpitDamageDealt || !OtherActor || OtherActor == this)
	{
		return;
	}
	if (SpitInstigator.IsValid() && OtherActor == SpitInstigator.Get())
	{
		return;
	}

	const float Damage = GetSpitDamage();
	if (Damage <= 0.f)
	{
		return;
	}

	if (ICombatTarget* Target = Cast<ICombatTarget>(OtherActor))
	{
		if (Target->IsCombatDowned())
		{
			return;
		}
		bSpitDamageDealt = true;
		Target->ApplyDamage(Damage, SpitInstigator.Get(), FName(TEXT("Spit")));
		UE_LOG(LogTemp, Display, TEXT("Devourable spit hit %s for %.1f (%s)"),
			*OtherActor->GetName(), Damage, *GetLabel());
	}
}

void ADevourable::OnSpitMeshHit(
	UPrimitiveComponent* /*HitComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& /*Hit*/)
{
	TryDealSpitDamage(OtherActor);
}

void ADevourable::OnSpitMeshBeginOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	TryDealSpitDamage(OtherActor);
}

void ADevourable::ReleaseFromGutToWorld(
	const FVector& WorldLocation,
	const FVector& LaunchVelocity,
	AActor* SpitInstigatorActor)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
		// Bugfix: spit was immediately re-devourable (CanDevour true + overlap). Grace ~1.5s.
		SpitGraceUntilTime = World->GetTimeSeconds() + FMath::Max(0.f, SpitReDevourGraceSeconds);
	}
	else
	{
		SpitGraceUntilTime = -1.f;
	}

	bInsideGut = false;
	bConsumed = false;

	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	SetActorLocation(WorldLocation);
	SetActorScale3D(HomeScale3D);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(true);
		MeshComponent->SetHiddenInGame(false);
		MeshComponent->SetRelativeScale3D(FVector::OneVector);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetGenerateOverlapEvents(true);
		MeshComponent->SetSimulatePhysics(LaunchVelocity.SizeSquared() > 1.f);
		MeshComponent->SetEnableGravity(true);
		if (LaunchVelocity.SizeSquared() > 1.f)
		{
			MeshComponent->SetPhysicsLinearVelocity(LaunchVelocity);
		}
		else
		{
			MeshComponent->SetSimulatePhysics(false);
		}
	}

	if (LaunchVelocity.SizeSquared() > 1.f)
	{
		ArmSpitProjectile(SpitInstigatorActor);
	}
	else
	{
		DisarmSpitProjectile();
	}

	UE_LOG(LogTemp, Display, TEXT("Devourable: spit grace %.2fs on %s (damage=%.1f)"),
		SpitReDevourGraceSeconds, *GetLabel(), GetSpitDamage());
}

void ADevourable::BeginRespawnCooldown()
{
	if (!bConsumed && !bInsideGut)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimer);
	const float Delay = FMath::Max(0.5f, RespawnCooldown);
	World->GetTimerManager().SetTimer(RespawnTimer, this, &ADevourable::OnRespawnTimer, Delay, false);
}

void ADevourable::OnRespawnTimer()
{
	RestoreScenePlacement();
}

void ADevourable::RestoreScenePlacement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}

	DisarmSpitProjectile();

	if (!bHomeStored)
	{
		StoreHome();
	}

	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	bConsumed = false;
	bInsideGut = false;
	SpitGraceUntilTime = -1.f;
	SetActorTransform(HomeTransform);
	SetActorScale3D(HomeScale3D);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetVisibility(true);
		MeshComponent->SetHiddenInGame(false);
		MeshComponent->SetRelativeScale3D(FVector::OneVector);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetGenerateOverlapEvents(true);
		MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	if (ACreatureAgent* Agent = Cast<ACreatureAgent>(this))
	{
		Agent->Revive();
	}
}
