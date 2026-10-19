#include "NestZone.h"
#include "SlimePawn.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ANestZone::ANestZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	// Spec meters: center (0,1.4,0), size (8,3,8). Unity Y-up → UE (X,Y,Z)=(z,x,y)*100
	// With nest at origin after axis convert: extent half-size (400,400,150), relative Z 140
	Trigger->SetBoxExtent(FVector(400.f, 400.f, 150.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
}

void ANestZone::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ANestZone::OnOverlapBegin);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ANestZone::OnOverlapEnd);
	Refresh();
}

void ANestZone::Refresh()
{
	ASlimePawn* SoftSlime = Cast<ASlimePawn>(UGameplayStatics::GetActorOfClass(GetWorld(), ASlimePawn::StaticClass()));
	if (!SoftSlime || !Trigger)
	{
		bPlayerInside = false;
		return;
	}

	const FVector CheckPoint = SoftSlime->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	bPlayerInside = Trigger->Bounds.GetBox().IsInside(CheckPoint);
}

void ANestZone::OnOverlapBegin(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ASlimePawn>(Other))
	{
		bPlayerInside = true;
	}
}

void ANestZone::OnOverlapEnd(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex)
{
	if (Cast<ASlimePawn>(Other))
	{
		bPlayerInside = false;
	}
}
