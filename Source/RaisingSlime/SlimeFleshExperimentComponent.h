#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeFleshExperimentComponent.generated.h"

class ADeformableSolverActor;
class AFleshActor;
class UFleshAsset;
class UJellyDeformerComponent;

/**
 * Spec §13 Chaos Flesh experiment scaffold.
 * Production look = soft-volume draped spring jelly only.
 * Flesh tet viz stays off unless both experiment + mesh flags are enabled.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeFleshExperimentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeFleshExperimentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Off by default — soft-volume jelly is the body. Opt-in for Flesh experiments. */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	bool bEnableFleshExperiment = false;

	/** Soft path; authored via Dataflow (StaticToTet / Radial). */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	FSoftObjectPath FleshAssetPath = FSoftObjectPath(TEXT("/Game/Flesh/FA_SlimeBody.FA_SlimeBody"));

	/**
	 * Draw Flesh tet ProceduralMesh. Default false — IsoStuffing reads as a ribbed
	 * hollow shell / double body. Never show unless intentionally debugging Flesh.
	 */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	bool bShowFleshMesh = false;

	/**
	 * Hide spring jelly only when Flesh mesh is shown AND sim is active.
	 * Ignored while bShowFleshMesh is false (jelly always stays).
	 */
	UPROPERTY(EditAnywhere, Category = "Flesh", meta = (EditCondition = "bShowFleshMesh"))
	bool bHideJellyWhenFleshActive = false;

	UPROPERTY(EditAnywhere, Category = "Flesh")
	float IncompressibilityMultiplier = 1.f;

	bool IsFleshSimulationActive() const { return bFleshActive; }

protected:
	void TryStartFlesh();
	void ShutdownFlesh();
	void SyncTransformToOwner();
	void EnforceProductionVisibility() const;
	void SetJellyVisible(bool bVisible) const;
	void SetFleshMeshVisible(bool bVisible) const;
	void AlignFleshToSlimeBody() const;
	void MakeFleshActorsMovable() const;
	bool HasTetrahedralRestCollection(const UFleshAsset* Asset) const;
	UFleshAsset* LoadFleshAsset() const;

	UPROPERTY()
	TObjectPtr<ADeformableSolverActor> SolverActor;

	UPROPERTY()
	TObjectPtr<AFleshActor> FleshActor;

	UPROPERTY()
	TObjectPtr<UFleshAsset> CachedFleshAsset;

	bool bFleshActive = false;
	bool bAttempted = false;
	bool bHidJelly = false;
};
