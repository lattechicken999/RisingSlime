#include "SlimeFleshExperimentComponent.h"
#include "JellyDeformerComponent.h"
#include "RaisingSlimeTypes.h"
#include "ChaosFlesh/FleshActor.h"
#include "ChaosFlesh/FleshComponent.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/ChaosDeformableSolverActor.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/ChaosDeformableTetrahedralComponent.h"
#include "ChaosFlesh/ChaosFleshCollectionFacade.h"
#include "ChaosFlesh/FleshCollection.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USlimeFleshExperimentComponent::USlimeFleshExperimentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bEnableFleshExperiment = false;
	bShowFleshMesh = false;
	bHideJellyWhenFleshActive = false;
}

void USlimeFleshExperimentComponent::BeginPlay()
{
	Super::BeginPlay();
	// Soft-volume jelly is the cohesive slime look.
	SetJellyVisible(true);
	SetFleshMeshVisible(false);

	if (bEnableFleshExperiment && bShowFleshMesh)
	{
		TryStartFlesh();
	}
	else
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: scaffold idle (enable=%d showMesh=%d) — soft-volume jelly only"),
			bEnableFleshExperiment ? 1 : 0, bShowFleshMesh ? 1 : 0);
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
	EnforceProductionVisibility();
	if (bFleshActive)
	{
		SyncTransformToOwner();
	}
}

void USlimeFleshExperimentComponent::EnforceProductionVisibility() const
{
	// Simulation tick can recreate Flesh viz PMC — keep it suppressed unless opted in.
	if (!bShowFleshMesh)
	{
		SetFleshMeshVisible(false);
		SetJellyVisible(true);
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

void USlimeFleshExperimentComponent::SetFleshMeshVisible(bool bVisible) const
{
	if (!FleshActor)
	{
		return;
	}

	FleshActor->SetActorHiddenInGame(!bVisible);
	FleshActor->SetActorEnableCollision(false);

	if (UFleshComponent* FleshComp = FleshActor->GetFleshComponent())
	{
		FleshComp->SetVisibility(bVisible, true);
		FleshComp->SetHiddenInGame(!bVisible, true);
		if (UProceduralMeshComponent* VizMesh = FleshComp->Mesh)
		{
			VizMesh->SetVisibility(bVisible);
			VizMesh->SetHiddenInGame(!bVisible);
			VizMesh->SetCastShadow(false);
		}
	}

	// Hide any leftover procedural children (sim may attach extra viz).
	TInlineComponentArray<UProceduralMeshComponent*> ProcMeshes(FleshActor);
	for (UProceduralMeshComponent* PMC : ProcMeshes)
	{
		if (PMC)
		{
			PMC->SetVisibility(bVisible);
			PMC->SetHiddenInGame(!bVisible);
			PMC->SetCastShadow(false);
		}
	}
}

void USlimeFleshExperimentComponent::AlignFleshToSlimeBody() const
{
	if (!FleshActor)
	{
		return;
	}
	FleshActor->SetActorRelativeLocation(FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm));
	FleshActor->SetActorRelativeRotation(FRotator::ZeroRotator);
	FleshActor->SetActorRelativeScale3D(FVector::OneVector);
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

	// Production path: never spawn deformable actors unless mesh viz is intentionally on.
	if (!bShowFleshMesh)
	{
		UE_LOG(LogTemp, Display, TEXT("SlimeFlesh: showMesh=false — skip solver/FleshActor spawn (jelly body)"));
		SetJellyVisible(true);
		return;
	}

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
			TEXT("SlimeFlesh: FA_SlimeBody missing — scaffold only. Soft-volume jelly stays primary."));
		SetJellyVisible(true);
		return;
	}
	if (!HasTetrahedralRestCollection(CachedFleshAsset))
	{
		UE_LOG(LogTemp, Display,
			TEXT("SlimeFlesh: FA_SlimeBody RestCollection empty/no tetrahedra — jelly stays primary."));
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

	MakeFleshActorsMovable();

	FleshActor->AttachToActor(Owner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	AlignFleshToSlimeBody();

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

	SetFleshMeshVisible(true);
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

	UE_LOG(LogTemp, Display,
		TEXT("SlimeFlesh: simulation+mesh enabled (jelly primary=%s, incompressibility=%.2f)"),
		bHideJellyWhenFleshActive ? TEXT("no") : TEXT("yes"),
		IncompressibilityMultiplier);
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
	if (SolverActor && GetOwner())
	{
		FTransform Xf = GetOwner()->GetActorTransform();
		Xf.AddToTranslation(Xf.TransformVectorNoScale(FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm)));
		SolverActor->SetActorTransform(Xf);
	}
}
