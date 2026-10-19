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
 * Visual primary is always the spring jelly PMC until Flesh mesh is
 * intentionally shown as a sphere-aligned soft body (not a tet pedestal).
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

	/** Soft path; authored via Dataflow (StaticToTet / Radial). */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	FSoftObjectPath FleshAssetPath = FSoftObjectPath(TEXT("/Game/Flesh/FA_SlimeBody.FA_SlimeBody"));

	/**
	 * Draw Flesh tet ProceduralMesh. Default false — FA RestCollection currently
	 * renders as a faceted tet volume under the jelly sphere (double body).
	 * Keep spring jelly as the only visible slime until Flesh is visual-ready.
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
