#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "JellyDeformerComponent.generated.h"

class UMaterialInterface;

/**
 * Viscous spring jelly — fluid-like traits without Niagara Fluids.
 * Length fields are meters unless named *Cm.
 */
USTRUCT(BlueprintType)
struct FJellySettings
{
	GENERATED_BODY()

	/** Soft / viscous springs. */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SpringStrength = 8.5f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Damping = 8.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float NeighborStrength = 3.6f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumDeformationM = 0.48f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumStretch = 1.9f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MovementDeformation = 1.7f;
	/** Lower = slower goo recovery. */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float RecoverySpeed = 0.38f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SquashStrength = 1.65f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float StretchStrength = 1.5f;

	/** Contact / flow pressure. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float CollisionDeformation = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float MaxContactDeformationM = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactRadiusM = 0.48f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactStrength = 1.35f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float ContactFalloff = 0.75f;
	/** How fast contact pressure rises/falls (1/s). Lower = more viscous, continuous climb. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float PressureSmoothRate = 7.f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float FlowSquash = 1.35f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float FlowStretch = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float TangentialSpread = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float StepUpSquash = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Flow")
	float IntoSpeedScale = 0.012f;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxImpactDeformationM = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundDeformationHeightM = 0.42f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactStrengthM = 0.11f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactFalloff = 0.85f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxGroundCompressionM = 0.14f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingImpactStrength = 0.024f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxLandingDeformationM = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingVelocityThresholdMps = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingBounceStrength = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingRecoverySpeed = 6.5f;

	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float VolumePreservation = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxRadialExpansionM = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxVerticalCompressionM = 0.24f;

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

	void BuildRestMesh();
	void EnsureMeshComponent();
	void WriteMesh();
	void ComputeNormals(TArray<FVector>& OutNormals) const;

	void GatherWorldContacts(const FVector& WorldVelocityCms);
	void UpdateSmoothedContacts(float Dt);
	void ApplyDirectionalFlow(const FVector& LocalFlowDir, float FlowAmount, float StepUpCm);
	void ApplySmoothedContacts();
	void BuildPose(const FVector& LocalVelocityCms);
	void ApplyGround(float Dt, bool bGrounded, float GroundImpactSpeedMps);
	void ApplyVolumePreservation();
	void ApplyInertia(const FVector& LocalAccelCms);
	void StepSprings(float Dt);
	void ClampPose();
	void ClampCurrentToRest();

	static float SmoothStep(float T);
	float MaxDeformCm() const { return Settings.MaximumDeformationM * 100.f; }
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
};
