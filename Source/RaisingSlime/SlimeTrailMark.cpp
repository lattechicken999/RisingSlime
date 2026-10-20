#include "SlimeTrailMark.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASlimeTrailMark::ASlimeTrailMark()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	MarkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkMesh"));
	SetRootComponent(MarkMesh);
	MarkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkMesh->SetGenerateOverlapEvents(false);
	MarkMesh->SetCastShadow(false);
	MarkMesh->SetReceivesDecals(false);
	MarkMesh->SetAbsolute(true, true, true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		MarkMesh->SetStaticMesh(PlaneMesh.Object);
	}
}

ASlimeTrailMark* ASlimeTrailMark::SpawnAt(
	UWorld* World,
	const FVector& WorldLocation,
	float ScaleCm,
	float LifetimeSeconds)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASlimeTrailMark* Mark = World->SpawnActor<ASlimeTrailMark>(
		ASlimeTrailMark::StaticClass(),
		WorldLocation + FVector(0.f, 0.f, 1.5f),
		FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f),
		Params);
	if (!Mark)
	{
		return nullptr;
	}

	Mark->Lifetime = FMath::Clamp(LifetimeSeconds, 0.25f, 2.5f);
	Mark->Age = 0.f;
	// Engine Plane is 100cm; BaseScale maps desired diameter in cm.
	Mark->BaseScale = FMath::Clamp(ScaleCm, 18.f, 90.f) / 100.f;
	Mark->SetActorScale3D(FVector(Mark->BaseScale, Mark->BaseScale, Mark->BaseScale));

	// Prefer M_Slime look; fall back to BasicShapeMaterial tinted slime-green.
	UMaterialInterface* SourceMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Slime.M_Slime"));
	if (!SourceMat)
	{
		SourceMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (SourceMat && Mark->MarkMesh)
	{
		Mark->MarkMid = UMaterialInstanceDynamic::Create(SourceMat, Mark);
		if (Mark->MarkMid)
		{
			// BasicShapeMaterial Color; harmless no-op if M_Slime has no such param.
			Mark->MarkMid->SetVectorParameterValue(
				TEXT("Color"),
				FLinearColor(0.42f, 0.90f, 0.32f, 0.55f));
			Mark->MarkMesh->SetMaterial(0, Mark->MarkMid);
		}
		else
		{
			Mark->MarkMesh->SetMaterial(0, SourceMat);
		}
	}

	return Mark;
}

void ASlimeTrailMark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / FMath::Max(0.05f, Lifetime), 0.f, 1.f);
	// Soft fade: hold briefly, then shrink + hide (no sideways spread).
	const float Fade = (T < 0.25f) ? 1.f : (1.f - (T - 0.25f) / 0.75f);
	const float S = BaseScale * FMath::Max(0.05f, Fade);
	SetActorScale3D(FVector(S, S, S * 0.15f));

	if (MarkMid)
	{
		const float A = 0.55f * Fade;
		MarkMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.90f, 0.32f, A));
		MarkMid->SetScalarParameterValue(TEXT("Opacity"), A);
	}

	if (Age >= Lifetime)
	{
		Destroy();
	}
}
