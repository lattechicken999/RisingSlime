#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "JellyDeformerComponent.generated.h"

class UMaterialInterface;

/** Spec §13 jelly settings. Length fields are meters (converted to cm in the solver). Soft contact squash retune. */
USTRUCT(BlueprintType)
struct FJellySettings
{
	GENERATED_BODY()

	/** Soft “물렁” springs — slow recovery, less rubber snap. */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SpringStrength = 12.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Damping = 6.8f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float NeighborStrength = 4.5f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumDeformationM = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumStretch = 1.85f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MovementDeformation = 1.55f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float RecoverySpeed = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SquashStrength = 1.55f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float StretchStrength = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float CollisionDeformation = 1.45f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxContactDeformationM = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxImpactDeformationM = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactRadiusM = 0.42f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactStrength = 1.75f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactFalloff = 0.85f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundDeformationHeightM = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactStrengthM = 0.13f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactFalloff = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxGroundCompressionM = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingImpactStrength = 0.028f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxLandingDeformationM = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingVelocityThresholdMps = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingBounceStrength = 1.2f;
	/** Lower = longer visible landing wobble. */
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingRecoverySpeed = 7.5f;

	/** Lightweight radial volume proxy (not Chaos Flesh). */
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float VolumePreservation = 0.62f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxRadialExpansionM = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Jelly|Volume")
	float MaxVerticalCompressionM = 0.22f;

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
	/** Probe WorldStatic / Dynamic / Pawn each jelly step for wall/floor/prey squash. */
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bWorldContactProbe = true;
};

/**
 * Spec §13 jelly spring body on UProceduralMeshComponent (1490 verts @ lat32/lon48).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API UJellyDeformerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJellyDeformerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Fixed-step jelly after movement. Velocities in cm/s (pawn space). */
	void Step(float Dt, const FVector& WorldVelocityCms, const FVector& WorldAccelCms, bool bGrounded, float GroundImpactSpeedMps);

	void ResetMotion();
	void AddImpulse(const FVector& LocalPoint, const FVector& LocalDirection, float Magnitude, float Radius, float Squash, float Stretch);

	/** Queue a contact dent in jelly mesh local space (PressureCm = inward displacement). */
	void EnqueueContact(const FVector& LocalPoint, const FVector& LocalNormal, float PressureCm, float RadiusCm);

	bool IsReady() const { return bReady; }
	UProceduralMeshComponent* GetMesh() const { return Mesh; }
	int32 GetVertexCount() const { return Rest.Num(); }

	void SetBodyScale(float InScale);
	void SetSlimeMaterial(UMaterialInterface* Material);

	UPROPERTY(EditAnywhere, Category = "Jelly")
	FJellySettings Settings;

	/** Spec §13 spare target: latitude 32 → (32-1)*48+2 = 1490. */
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
		float PressureCm = 0.f;
		float RadiusCm = 0.f;
	};

	void BuildRestMesh();
	void EnsureMeshComponent();
	void WriteMesh();
	void ComputeNormals(TArray<FVector>& OutNormals) const;

	void GatherWorldContacts(const FVector& WorldVelocityCms);
	void ApplyQueuedContacts();
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

	TArray<FPendingContact> ActiveContacts;
	TArray<FPendingContact> PendingContacts;

	float BodyScale = 1.f;
	float LandingPress = 0.f;
	float LandingVelocity = 0.f;
	bool bWasGrounded = false;
	bool bReady = false;
	bool bAscending = false;

	FVector PrevWorldVelocityCms = FVector::ZeroVector;

	static constexpr int32 MaxContactsPerStep = 10;
};
