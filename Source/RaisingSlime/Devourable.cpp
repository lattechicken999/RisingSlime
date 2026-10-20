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
	if (bConsumed || IsHidden())
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
	bHomeStored = true;
}

void ADevourable::Consume()
{
	bConsumed = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ADevourable::BeginRespawnCooldown()
{
	if (!bConsumed)
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

	bConsumed = false;
	SetActorTransform(HomeTransform);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	if (ACreatureAgent* Agent = Cast<ACreatureAgent>(this))
	{
		Agent->Revive();
	}
}
