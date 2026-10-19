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
 * Does not replace gameplay collision / spring jelly until a valid FleshAsset exists.
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

	/** When Flesh rest collection is valid, hide procedural jelly mesh. */
	UPROPERTY(EditAnywhere, Category = "Flesh")
	bool bHideJellyWhenFleshActive = true;

	UPROPERTY(EditAnywhere, Category = "Flesh")
	float IncompressibilityMultiplier = 1.f;

	bool IsFleshSimulationActive() const { return bFleshActive; }

protected:
	void TryStartFlesh();
	void ShutdownFlesh();
	void SyncTransformToOwner();
	UFleshAsset* LoadFleshAsset() const;

	UPROPERTY()
	TObjectPtr<ADeformableSolverActor> SolverActor;

	UPROPERTY()
	TObjectPtr<AFleshActor> FleshActor;

	UPROPERTY()
	TObjectPtr<UFleshAsset> CachedFleshAsset;

	bool bFleshActive = false;
	bool bAttempted = false;
};
