#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "JellyDeformerComponent.generated.h"

class UMaterialInterface;

/** Spec §13 jelly settings. Length fields are meters (converted to cm in the solver). */
USTRUCT(BlueprintType)
struct FJellySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SpringStrength = 30.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Damping = 3.8f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float NeighborStrength = 12.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumDeformationM = 0.28f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaximumStretch = 1.65f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MovementDeformation = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float RecoverySpeed = 1.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float SquashStrength = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float StretchStrength = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxContactDeformationM = 0.07f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactRadiusM = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactStrength = 1.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float ContactFalloff = 1.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundDeformationHeightM = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactStrengthM = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float GroundContactFalloff = 1.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxGroundCompressionM = 0.06f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingImpactStrength = 0.012f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float MaxLandingDeformationM = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingVelocityThresholdMps = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingBounceStrength = 1.f;
	UPROPERTY(EditAnywhere, Category = "Jelly")
	float LandingRecoverySpeed = 16.f;

	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bContactDeformation = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bGroundContact = true;
	UPROPERTY(EditAnywhere, Category = "Jelly|Features")
	bool bLandingImpact = true;
};

/**
 * Spec §13 jelly spring body on UProceduralMeshComponent (738 verts @ lat24/lon32).
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

	bool IsReady() const { return bReady; }
	UProceduralMeshComponent* GetMesh() const { return Mesh; }

	void SetBodyScale(float InScale);
	void SetSlimeMaterial(UMaterialInterface* Material);

	UPROPERTY(EditAnywhere, Category = "Jelly")
	FJellySettings Settings;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	int32 Latitude = 24;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	int32 Longitude = 32;

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
		float Pressure = 0.f;
		float Radius = 0.f;
	};

	void BuildRestMesh();
	void EnsureMeshComponent();
	void WriteMesh();
	void ComputeNormals(TArray<FVector>& OutNormals) const;

	void BuildPose(const FVector& LocalVelocityCms);
	void ApplyGround(float Dt, bool bGrounded, float GroundImpactSpeedMps);
	void ApplyInertia(const FVector& LocalAccelCms);
	void StepSprings(float Dt);
	void ClampPose();
	void ClampCurrentToRest();

	static float SmoothStep(float T);
	float MaxDeformCm() const { return Settings.MaximumDeformationM * 100.f; }

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
};
