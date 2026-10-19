#include "SlimeFleshExperimentComponent.h"
#include "JellyDeformerComponent.h"
#include "ChaosFlesh/FleshActor.h"
#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/ChaosDeformableSolverActor.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/ChaosDeformableTetrahedralComponent.h"
#include "ChaosFlesh/ChaosFleshCollectionFacade.h"
#include "ChaosFlesh/FleshCollection.h"
#include "Components/SceneComponent.h"
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
	// Always ensure jelly is visible before attempting Flesh (empty shells used to hide it).
	SetJellyVisible(true);
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

bool USlimeFleshExperimentComponent::HasTetrahedralRestCollection(const UFleshAsset* Asset) const
{
	if (!Asset)
	{
		return false;
	}
	// UFleshAsset always constructs an empty FFleshCollection — SharedPtr IsValid is NOT enough.
	const TSharedPtr<const FFleshCollection> Collection = Asset->GetFleshCollection();
	if (!Collection.IsValid())
	{
		return false;
	}
	const Chaos::FFleshCollectionFacade Facade(*Collection);
	return Facade.IsTetrahedronValid() && Facade.NumVertices() > 0;
}

void USlimeFleshExperimentComponent::SetJellyVisible(bool bVisible) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (UJellyDeformerComponent* Jelly = Owner->FindComponentByClass<UJellyDeformerComponent>())
	{
		if (UProceduralMeshComponent* Mesh = Jelly->GetMesh())
		{
			Mesh->SetVisibility(bVisible);
			Mesh->SetHiddenInGame(!bVisible);
		}
	}
}

void USlimeFleshExperimentComponent::MakeFleshActorsMovable() const
{
	auto MakeMovable = [](AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		TInlineComponentArray<USceneComponent*> SceneComps(Actor);
		for (USceneComponent* Comp : SceneComps)
		{
			if (Comp && Comp->Mobility != EComponentMobility::Movable)
			{
				Comp->SetMobility(EComponentMobility::Movable);
			}
		}
	};

	MakeMovable(SolverActor);
	MakeMovable(FleshActor);

	if (SolverActor)
	{
		if (UDeformableSolverComponent* SolverComp = SolverActor->GetDeformableSolverComponent())
		{
			SolverComp->SetMobility(EComponentMobility::Movable);
		}
	}
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
	if (!CachedFleshAsset)
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: FA_SlimeBody missing — scaffold only. Run Content/Python/create_flesh_scaffold.py then author tet mesh via Dataflow (DF_StaticToTetMesh / Radial). Spring jelly stays primary."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f, FColor::Orange,
				TEXT("SlimeFlesh: asset missing — jelly remains (see Output Log)"));
		}
		SetJellyVisible(true);
		return;
	}
	if (!HasTetrahedralRestCollection(CachedFleshAsset))
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: FA_SlimeBody RestCollection empty/no tetrahedra — finish Dataflow tetmeshing. Spring jelly stays primary."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f, FColor::Orange,
				TEXT("SlimeFlesh: empty collection — jelly remains"));
		}
		SetJellyVisible(true);
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
		SetJellyVisible(true);
		return;
	}

	// Engine defaults DeformableSolver root to Static; slime pawns move every frame.
	MakeFleshActorsMovable();

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
		SetJellyVisible(false);
		bHidJelly = true;
	}
	else
	{
		SetJellyVisible(true);
		bHidJelly = false;
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
	if (bHidJelly)
	{
		SetJellyVisible(true);
		bHidJelly = false;
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
