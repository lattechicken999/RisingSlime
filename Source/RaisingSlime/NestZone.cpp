#include "NestZone.h"
#include "SlimePawn.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ANestZone::ANestZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	// Visual/pad companion only — digest gating removed (field absorb).
	Trigger->SetBoxExtent(FVector(400.f, 400.f, 150.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetGenerateOverlapEvents(false);
}

void ANestZone::BeginPlay()
{
	Super::BeginPlay();
	bPlayerInside = false;
}

void ANestZone::Refresh()
{
	// Nest zone no longer gates gameplay.
	bPlayerInside = false;
}

void ANestZone::OnOverlapBegin(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	bPlayerInside = false;
}

void ANestZone::OnOverlapEnd(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex)
{
	bPlayerInside = false;
}
