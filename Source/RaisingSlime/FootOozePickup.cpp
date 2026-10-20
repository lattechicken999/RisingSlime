#include "FootOozePickup.h"
#include "SlimePawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AFootOozePickup::AFootOozePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(50.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Collision);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.28f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
}

void AFootOozePickup::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetActorLocation();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AFootOozePickup::OnOverlapBegin);
	SetAvailable(true);
}

void AFootOozePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AFootOozePickup::OnOverlapBegin(
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

	if (!bAvailable)
	{
		return;
	}
	if (ASlimePawn* Slime = Cast<ASlimePawn>(OtherActor))
	{
		Collect(Slime);
	}
}

void AFootOozePickup::Collect(ASlimePawn* SoftSlime)
{
	if (!SoftSlime)
	{
		return;
	}

	SoftSlime->SetFootTrailFromItem(true);
	UE_LOG(LogTemp, Display,
		TEXT("FootTrailPickup: collected — slime trail marks while grounded+moving (no SW_Drop; item gate only)"));
	SetAvailable(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
		World->GetTimerManager().SetTimer(
			RespawnTimer, this, &AFootOozePickup::OnRespawnTimer,
			FMath::Max(1.f, RespawnCooldown), false);
	}
}

void AFootOozePickup::OnRespawnTimer()
{
	SetActorLocation(HomeLocation);
	SetAvailable(true);
	UE_LOG(LogTemp, Log, TEXT("FootOozePickup: respawned"));
}

void AFootOozePickup::SetAvailable(bool bInAvailable)
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
