#include "Devourable.h"
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

bool ADevourable::CanDevour() const
{
	if (bConsumed || bInsideGut || IsHidden())
	{
		return false;
	}

	if (Kind != EPreyKind::Creature)
	{
		return true;
	}

	if (const ACreatureAgent* Agent = Cast<ACreatureAgent>(this))
	{
		return Agent->IsDowned();
	}
	return false;
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
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	SetActorHiddenInGame(false);
	if (MeshComponent)
	{
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
	SetActorTransform(HomeTransform);
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
	}

	if (ACreatureAgent* Agent = Cast<ACreatureAgent>(this))
	{
		Agent->Revive();
	}
}
