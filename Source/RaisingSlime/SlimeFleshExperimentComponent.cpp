#include "SlimeFleshExperimentComponent.h"
#include "JellyDeformerComponent.h"
#include "ChaosFlesh/FleshActor.h"
#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/ChaosDeformableSolverActor.h"
#include "ChaosFlesh/ChaosDeformableTetrahedralComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USlimeFleshExperimentComponent::USlimeFleshExperimentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USlimeFleshExperimentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bEnableFleshExperiment)
	{
		TryStartFlesh();
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("SlimeFlesh: experiment disabled (spring jelly remains primary)"));
	}
}

void USlimeFleshExperimentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownFlesh();
	Super::EndPlay(EndPlayReason);
}

void USlimeFleshExperimentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bFleshActive)
	{
		SyncTransformToOwner();
	}
}

UFleshAsset* USlimeFleshExperimentComponent::LoadFleshAsset() const
{
	if (CachedFleshAsset)
	{
		return CachedFleshAsset;
	}
	if (!FleshAssetPath.IsValid())
	{
		return nullptr;
	}
	return Cast<UFleshAsset>(FleshAssetPath.TryLoad());
}

void USlimeFleshExperimentComponent::TryStartFlesh()
{
	if (bAttempted)
	{
		return;
	}
	bAttempted = true;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	CachedFleshAsset = LoadFleshAsset();
	const bool bHasCollection = CachedFleshAsset && CachedFleshAsset->GetFleshCollection().IsValid();
	if (!CachedFleshAsset)
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: FA_SlimeBody missing — scaffold only. Run Content/Python/create_flesh_scaffold.py then author tet mesh via Dataflow (DF_StaticToTetMesh / Radial). Spring jelly stays primary."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f, FColor::Orange,
				TEXT("SlimeFlesh: asset missing — jelly remains (see Output Log)"));
		}
		return;
	}
	if (!bHasCollection)
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: FA_SlimeBody loaded but RestCollection empty — finish Dataflow tetmeshing. Spring jelly stays primary."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f, FColor::Orange,
				TEXT("SlimeFlesh: empty collection — jelly remains"));
		}
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SolverActor = World->SpawnActor<ADeformableSolverActor>(Owner->GetActorLocation(), Owner->GetActorRotation(), Params);
	FleshActor = World->SpawnActor<AFleshActor>(Owner->GetActorLocation(), Owner->GetActorRotation(), Params);
	if (!SolverActor || !FleshActor || !FleshActor->GetFleshComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("SlimeFlesh: failed to spawn solver/flesh actors"));
		ShutdownFlesh();
		return;
	}

	FleshActor->AttachToActor(Owner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	FleshActor->GetFleshComponent()->SetRestCollection(CachedFleshAsset);
	FleshActor->GetFleshComponent()->BodyForces.bApplyGravity = true;
	FleshActor->GetFleshComponent()->BodyForces.IncompressibilityMultiplier = IncompressibilityMultiplier;
	FleshActor->GetFleshComponent()->SimulationSpace.SimSpace = ChaosDeformableSimSpace::ComponentXf;

	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Slime.M_Slime")))
	{
		FleshActor->GetFleshComponent()->SetMaterial(0, Mat);
	}

	FleshActor->PrimarySolver = SolverActor;
	FleshActor->EnableSimulation(SolverActor);

	bFleshActive = true;
	UE_LOG(LogTemp, Display, TEXT("SlimeFlesh: simulation enabled (Component space, incompressibility=%.2f)"), IncompressibilityMultiplier);

	if (bHideJellyWhenFleshActive)
	{
		if (UJellyDeformerComponent* Jelly = Owner->FindComponentByClass<UJellyDeformerComponent>())
		{
			if (UProceduralMeshComponent* Mesh = Jelly->GetMesh())
			{
				Mesh->SetVisibility(false);
				Mesh->SetHiddenInGame(true);
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor::Green, TEXT("SlimeFlesh: ACTIVE"));
	}
}

void USlimeFleshExperimentComponent::ShutdownFlesh()
{
	bFleshActive = false;
	if (FleshActor)
	{
		FleshActor->Destroy();
		FleshActor = nullptr;
	}
	if (SolverActor)
	{
		SolverActor->Destroy();
		SolverActor = nullptr;
	}
}

void USlimeFleshExperimentComponent::SyncTransformToOwner()
{
	// Attached FleshActor follows owner; keep solver colocated for world queries.
	if (SolverActor && GetOwner())
	{
		SolverActor->SetActorTransform(GetOwner()->GetActorTransform());
	}
}
