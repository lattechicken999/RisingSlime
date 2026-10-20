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

void ADevourable::Consume()
{
	bConsumed = true;
	// Stay visible — gut attach + float is the primary devour read. No pop-hide.
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ADevourable::AttachIntoGut(USceneComponent* GutRoot)
{
	if (!GutRoot)
	{
		return;
	}

	bInsideGut = true;
	Consume();
	AttachToComponent(GutRoot, FAttachmentTransformRules::KeepWorldTransform);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(true);
		MeshComponent->SetHiddenInGame(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
		// Restore mesh scale for next spawn (dissolve may have shrunk it).
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
