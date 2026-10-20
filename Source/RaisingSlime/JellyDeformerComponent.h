#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "JellyDeformerComponent.generated.h"

class UMaterialInterface;

/**
 * Viscous liquid body (not bouncy jelly) — thick goo without Niagara Fluids.
 * Length fields are meters unless named *Cm.
 */
USTRUCT(BlueprintType)
struct FJellySettings
{
	GENERATED_BODY()

	/**
	 * Weak restoring force. Keep tiny — shape returns by viscous creep, not snap.
	 * Effective K = SpringStrength * RecoverySpeed.
	 */
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float SpringStrength = 2.2f;
	/** Velocity kill (overdamped). Aim ≫ 2*sqrt(K). */
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float Damping = 16.f;
	/** ζ≥1 overdamps residual spring; 2+ reads as thick liquid. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float DampingRatio = 2.4f;
	/** Exponential creep rate toward pose (1/s). Primary recovery path. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float ViscosityCreepRate = 2.8f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float NeighborStrength = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumDeformationM = 0.5f;
	/** Hard cap on height scale vs rest (medium elongate; volume rad keeps waist). */
	UPROPERTY(EditAnywhere, Category = "Jelly", meta = (ClampMin = "1.05", ClampMax = "1.8"))
	float MaximumStretch = 1.55f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MovementDeformation = 1.55f;
	/** Scales residual spring K; keep low for languid ooze. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Viscosity")
	float RecoverySpeed = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SquashStrength = 1.7f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float StretchStrength = 1.35f;

	/** Max +height while rising (fraction of rest). Medium stretch, not needle. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Jump", meta = (ClampMin = "0.05", ClampMax = "0.7"))
	float JumpRiseStretch = 0.55f;
	/** Max height loss while falling (fraction). */
	UPROPERTY(EditAnywhere, Category = "Jelly|Jump", meta = (ClampMin = "0.05", ClampMax = "0.4"))
	float JumpFallSquash = 0.2f;
	/** Brief flatten on takeoff impulse (fraction height loss). */
	UPROPERTY(EditAnywhere, Category = "Jelly|Jump", meta = (ClampMin = "0", ClampMax = "0.25"))
	float JumpTakeoffFlatten = 0.08f;
	/** |Vz| (m/s) that maps to full jump deform. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Jump")
	float JumpDeformRefSpeedMps = 7.2f;

	/** Contact / flow pressure — stick then ooze. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float CollisionDeformation = 1.05f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float MaxContactDeformationM = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactRadiusM = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactStrength = 1.25f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactFalloff = 0.7f;
	/** Slow pressure rise/fall so contact spreads instead of popping. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float PressureSmoothRate = 3.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float FlowSquash = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float FlowStretch = 1.05f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float TangentialSpread = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float StepUpSquash = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float IntoSpeedScale = 0.01f;

	/**
	 * Underside engulf: obstacle height above feet must be < this × body *diameter*
	 * (Hunt Prey_Rock ≈55–70cm; diameter≈100cm → 0.9 works). Walls stay face-squash.
	 */
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	bool bUndersideEngulf = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf", meta = (ClampMin = "0.3", ClampMax = "1.2"))
	float EngulfMaxHeightFractionOfDiameter = 0.95f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfMinHeightCm = 3.f;
	/** Minimum cavity lift (cm) so wrap is visible even on short bumps. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfMinWrapCm = 14.f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfWrapStrength = 1.85f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfLateralOoze = 1.05f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfInfluenceRadiusScale = 0.95f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfSmoothRate = 8.f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	float EngulfStepUpBoost = 0.75f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Engulf")
	bool bEngulfDebugLog = true;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxImpactDeformationM = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundDeformationHeightM = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactStrengthM = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactFalloff = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxGroundCompressionM = 0.16f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingImpactStrength = 0.03f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxLandingDeformationM = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingVelocityThresholdMps = 0.9f;
	/** Near-zero: landings flatten/flow, do not rebound. */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingBounceStrength = 0.08f;
	/** Slow exponential settle of landing squash (1/s). */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingRecoverySpeed = 2.4f;

	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float VolumePreservation = 0.78f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxRadialExpansionM = 0.14f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxVerticalCompressionM = 0.26f;
	/** How fast volume ooze blends (1/s). */
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float VolumeOozeRate = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bContactDeformation = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bGroundContact = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bLandingImpact = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bVolumePreservation = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bRadialExpansion = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bWorldContactProbe = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bDirectionalFlow = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bUndersideEngulfFeature = true;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API UJellyDeformerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJellyDeformerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Fixed-step jelly. Velocities cm/s. MoveIntent = desired planar velocity (world).
	 * StepUpCm = lift from movement step-assist this tick.
	 */
	void Step(float Dt, const FVector& WorldVelocityCms, const FVector& WorldAccelCms, bool bGrounded, float GroundImpactSpeedMps,
		const FVector& WorldMoveIntentCms = FVector::ZeroVector, float StepUpCm = 0.f);

	void ResetMotion();
	void AddImpulse(const FVector& LocalPoint, const FVector& LocalDirection, float Magnitude, float Radius, float Squash, float Stretch);
	void EnqueueContact(const FVector& LocalPoint, const FVector& LocalNormal, float PressureCm, float RadiusCm, float IntoSpeedCms = 0.f);

	bool IsReady() const { return bReady; }
	UProceduralMeshComponent* GetMesh() const { return Mesh; }
	int32 GetVertexCount() const { return Rest.Num(); }

	void SetBodyScale(float InScale);
	void SetSlimeMaterial(UMaterialInterface* Material);

	UPROPERTY(EditAnywhere, Category = "Jelly")
	FJellySettings Settings;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	int32 Latitude = 32;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	int32 Longitude = 48;

protected:
	struct FEdge
	{
		int32 A = 0;
		int32 B = 0;
		float RestLength = 0.f;
	};

	struct FPendingContact
	{
		FVector LocalPoint = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		float TargetPressureCm = 0.f;
		float RadiusCm = 0.f;
		float IntoSpeedCms = 0.f;
	};

	/** Temporally smoothed contact field for continuous climb/squash. */
	struct FSmoothedContact
	{
		FVector LocalPoint = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		float PressureCm = 0.f;
		float TargetPressureCm = 0.f;
		float RadiusCm = 0.f;
		float IntoSpeedCms = 0.f;
		float Age = 0.f;
	};

	/** Low obstacle under the body — underside cavity wrap / engulf. */
	struct FEngulfSite
	{
		FVector LocalTop = FVector::ZeroVector;
		float HeightCm = 0.f;
		float RadiusCm = 0.f;
		float Strength = 0.f;
		float TargetStrength = 0.f;
		float Age = 0.f;
	};

	void BuildRestMesh();
	void EnsureMeshComponent();
	void WriteMesh();
	void ComputeNormals(TArray<FVector>& OutNormals) const;

	void GatherWorldContacts(const FVector& WorldVelocityCms);
	void GatherEngulfObstacles();
	void UpdateSmoothedContacts(float Dt);
	void UpdateSmoothedEngulfs(float Dt, float StepUpCm);
	void ApplyDirectionalFlow(const FVector& LocalFlowDir, float FlowAmount, float StepUpCm);
	void ApplySmoothedContacts();
	void ApplyEngulfWrap();
	/** Displace verts from soft-volume particle field (primary collision paradigm). */
	void ApplySoftVolumeSkin();
	/** Retired stub (surface-drift replaced glue sink skin). */
	void ApplyGlueSinkSkin();
	void BuildPose(const FVector& LocalVelocityCms, const FVector& LocalAccelCms);
	void ApplyGround(float Dt, bool bGrounded, float GroundImpactSpeedMps);
	void ApplyVolumePreservation();
	void ApplyInertia(const FVector& LocalAccelCms);
	void StepSprings(float Dt);
	void ClampPose();
	void ClampCurrentToRest();

	static float SmoothStep(float T);
	float MaxDeformCm() const { return Settings.MaximumDeformationM * 100.f; }
	/** Cap vertical offset so jump never needles the shell. */
	float MaxVerticalStretchCm() const { return RestRadiusCm() * (Settings.MaximumStretch - 1.f); }
	float RestRadiusCm() const { return 50.f; }

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SlimeMaterial;

	TArray<FVector> Rest;
	TArray<FVector> Pose;
	TArray<FVector> Current;
	TArray<FVector> Velocity;
	TArray<float> Influence;
	TArray<FEdge> Edges;
	TArray<int32> Triangles;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector> RenderNormals;

	TArray<FPendingContact> PendingContacts;
	TArray<FSmoothedContact> SmoothedContacts;
	TArray<FEngulfSite> PendingEngulfs;
	TArray<FEngulfSite> SmoothedEngulfs;

	float BodyScale = 1.f;
	float LandingPress = 0.f;
	float LandingVelocity = 0.f;
	float SmoothedFlowAmount = 0.f;
	FVector SmoothedFlowDir = FVector::ZeroVector;
	bool bWasGrounded = false;
	bool bReady = false;
	bool bAscending = false;

	FVector PrevWorldVelocityCms = FVector::ZeroVector;

	static constexpr int32 MaxContactsPerStep = 8;
	static constexpr int32 MaxEngulfsPerStep = 4;
};
