#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSoftVolumeComponent.generated.h"

class ASlimePawn;
class AActor;

/**
 * Soft-volume collision: overlapping spheres that resolve WorldStatic individually.
 * Obstacle creep climb: particles viscous-creep onto ledges under WASD press (no hop).
 * Prey engulf climb sleeve is retired (surface-drift gut owns devour).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeSoftVolumeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeSoftVolumeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RebuildRest(float BodyRadiusCm);
	void EnsureRadius(float BodyRadiusCm);
	void ResetToActor();
	void TeleportWithActor();

	bool StepMove(ASlimePawn* Pawn, const FVector& DesiredDelta, float Dt, bool& bOutGrounded);

	int32 GetParticleCount() const { return Particles.Num(); }
	int32 GetSupportedParticleCount() const;
	FVector GetParticleWorldPos(int32 Index) const;
	float GetParticleRadius(int32 Index) const;
	FVector GetBodyCenterWorld() const;
	float GetBodyRadiusCm() const { return BodyRadiusCm; }

	bool ContainsWorldPoint(const FVector& WorldPos) const;
	float SampleCoverage(const TArray<FVector>& SamplePoints) const;

	/** Retired — no-op (Contained uses gut Sink01 glue). */
	void SetEngulfClimbTarget(AActor* Prey);
	void ClearEngulfClimbTarget();
	bool HasEngulfClimbTarget() const { return false; }
	float GetEngulfClimbProgress01() const { return 0.f; }

	FVector SampleDisplacementLocal(const FVector& MeshLocalPoint) const;

	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0.8", ClampMax = "2.5"))
	float SkinMacroBoost = 1.75f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0", ClampMax = "1"))
	float SkinLateralScale = 0.15f;

	/** Extra lateral skin while climb-wrapping (ferrofluid sleeve read). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0", ClampMax = "1"))
	float EngulfSkinLateralBoost = 0.55f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float SkinSigmaScale = 0.45f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0", ClampMax = "1"))
	float SkinPlanarBias = 0.9f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float FormationCreepRate = 6.5f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity", meta = (ClampMin = "0", ClampMax = "1"))
	float FormationVerticalScale = 0.12f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float VelocityDamping = 14.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float NeighborSoftness = 0.35f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity", meta = (ClampMin = "0", ClampMax = "1"))
	float NeighborVerticalScale = 0.18f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Support")
	float SupportSettleRate = 10.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Support")
	float SupportProbeExtraCm = 14.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	float GroundNormalZ = 0.45f;

	/** Max lip height particles may viscous-creep onto (fraction of body radius). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|CreepClimb", meta = (ClampMin = "0.2", ClampMax = "1.4"))
	float CreepClimbMaxHeightFrac = 0.95f;

	/** Overdamped climb onto ledge tops while pressing (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|CreepClimb", meta = (ClampMin = "0.5", ClampMax = "12"))
	float CreepClimbRate = 5.5f;

	/** How strongly ledge-supported particles keep XY on the obstacle (vs formation). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|CreepClimb", meta = (ClampMin = "0", ClampMax = "1"))
	float CreepClimbStickXY = 0.72f;

	/** How fast climb front rises while pressing into prey (retired). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float EngulfClimbAdvanceRate = 0.55f;

	/** Viscous follow onto prey surface sleeve (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0.5", ClampMax = "12"))
	float EngulfClimbCreepRate = 4.2f;

	/** Climb front decays when player backs off (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0", ClampMax = "2"))
	float EngulfClimbDecayRate = 0.35f;

protected:
	struct FParticle
	{
		FVector RestLocal = FVector::ZeroVector;
		FVector WorldPos = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Radius = 20.f;
		float RestDistToCore = 0.f;
		FVector SupportCenter = FVector::ZeroVector;
		bool bHasSupport = false;
		bool bClimbWrapping = false;
		/** Particle is viscous-creeping / resting on an elevated obstacle lip. */
		bool bObstacleCreeping = false;
	};

	struct FEngulfClimb
	{
		TWeakObjectPtr<AActor> Prey;
		FVector Center = FVector::ZeroVector;
		float RadiusXY = 30.f;
		float BottomZ = 0.f;
		float TopZ = 80.f;
		float ClimbFront01 = 0.f;
		bool bActive = false;
	};

	void BuildDefaultLayout(float BodyRadiusCm);
	FVector BodyCenterFromActor(const AActor* Actor) const;
	bool SweepParticle(AActor* Ignore, const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const;
	void ProbeSupports(AActor* Ignore, bool& bOutGrounded);
	void ApplyObstacleCreepClimb(AActor* Ignore, const FVector& DesiredDelta, float Dt, bool& bOutGrounded);
	void ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter);
	void SettleToSupports(float Dt);
	void ApplyEngulfClimb(ASlimePawn* Pawn, float Dt, const FQuat& YawQ);
	void SyncActorFromParticles(ASlimePawn* Pawn);
	void RefreshClimbBoundsFromPrey();
	float EstimateFloorSupportZ() const;

	TArray<FParticle> Particles;
	FEngulfClimb Climb;
	FVector RestComLocal = FVector::ZeroVector;
	float BodyRadiusCm = 50.f;
	float BodyHeightCm = 52.f;
};
