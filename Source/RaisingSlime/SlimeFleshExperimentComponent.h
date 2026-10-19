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
 * Does not replace gameplay collision / spring jelly until a valid FleshAsset
 * with tetrahedra exists. Empty FA_SlimeBody shells must leave jelly visible.
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

	UPROPERTY(EditAnywhere, Category = "Flesh")
	bool bEnableFleshExperiment = true;

	/** Soft path; authored via Dataflow (StaticToTet / Radial) later. */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	FSoftObjectPath FleshAssetPath = FSoftObjectPath(TEXT("/Game/Flesh/FA_SlimeBody.FA_SlimeBody"));

	/**
	 * Hide procedural jelly only when Flesh tet sim is actually running.
	 * Default false: empty RestCollection shells must never blank the slime body.
	 */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	bool bHideJellyWhenFleshActive = false;

	UPROPERTY(EditAnywhere, Category = "Flesh")
	float IncompressibilityMultiplier = 1.f;

	bool IsFleshSimulationActive() const { return bFleshActive; }

protected:
	void TryStartFlesh();
	void ShutdownFlesh();
	void SyncTransformToOwner();
	void SetJellyVisible(bool bVisible) const;
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
