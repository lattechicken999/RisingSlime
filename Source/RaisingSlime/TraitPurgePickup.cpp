#include "TraitPurgePickup.h"
#include "ExpeditionDirector.h"
#include "SlimePawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ATraitPurgePickup::ATraitPurgePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(55.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Collision);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.55f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	if (ConeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(ConeMesh.Object);
	}
}

void ATraitPurgePickup::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ATraitPurgePickup::OnOverlapBegin);
	SetAvailable(true);
}

void ATraitPurgePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void ATraitPurgePickup::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComp;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!bAvailable || !OtherActor)
	{
		return;
	}
	if (!Cast<ASlimePawn>(OtherActor))
	{
		return;
	}
	Collect();
}

void ATraitPurgePickup::Collect()
{
	AExpeditionDirector* Director = Cast<AExpeditionDirector>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AExpeditionDirector::StaticClass()));
	if (!Director)
	{
		return;
	}

	Director->SetHasTraitPurgeItem(true);
	UE_LOG(LogTemp, Display, TEXT("TraitPurgePickup: collected — Tab delete unlocked"));
	SetAvailable(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
		World->GetTimerManager().SetTimer(
			RespawnTimer, this, &ATraitPurgePickup::OnRespawnTimer,
			FMath::Max(1.f, RespawnCooldown), false);
	}
}

void ATraitPurgePickup::OnRespawnTimer()
{
	SetActorLocation(HomeLocation);
	SetAvailable(true);
	UE_LOG(LogTemp, Log, TEXT("TraitPurgePickup: respawned"));
}

void ATraitPurgePickup::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
	SetActorHiddenInGame(!bAvailable);
	SetActorEnableCollision(bAvailable);
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(bAvailable);
		Collision->SetCollisionEnabled(bAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(bAvailable);
		MeshComponent->SetHiddenInGame(!bAvailable);
	}
}
