#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSoftVolumeComponent.generated.h"

class ASlimePawn;

/**
 * Soft-volume collision: overlapping spheres that resolve WorldStatic individually
 * so thin poles can sit between/under particles instead of pedestalizing one hard sphere.
 * Per-particle support heights allow the body to drape across obstacle edges.
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
	/** Rebuild particle layout only when radius changed meaningfully. */
	void EnsureRadius(float BodyRadiusCm);
	void ResetToActor();
	void TeleportWithActor();

	/** Integrate soft volume with desired actor delta (cm). Returns grounded if any lower particle supports. */
	bool StepMove(ASlimePawn* Pawn, const FVector& DesiredDelta, float Dt, bool& bOutGrounded);

	int32 GetParticleCount() const { return Particles.Num(); }
	int32 GetSupportedParticleCount() const;
	FVector GetParticleWorldPos(int32 Index) const;
	float GetParticleRadius(int32 Index) const;
	FVector GetBodyCenterWorld() const;
	float GetBodyRadiusCm() const { return BodyRadiusCm; }

	/** True if WorldPos lies inside any soft-volume particle sphere (or body fallback). */
	bool ContainsWorldPoint(const FVector& WorldPos) const;

	/**
	 * Fraction of SamplePoints inside the soft volume [0..1].
	 * Used for player-driven engulf coverage.
	 */
	float SampleCoverage(const TArray<FVector>& SamplePoints) const;

	/**
	 * Local-space displacement (cm) for PMC skinning at mesh-local point (scaled cm).
	 * Macro = wide Gaussian support-height (Z). Micro XY stays weak (no IDW spikes).
	 */
	FVector SampleDisplacementLocal(const FVector& MeshLocalPoint) const;

	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	bool bEnabled = true;

	/** Off by default — use console `slime.SoftVolume.DebugDraw 1` or ToggleDebug. */
	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	bool bDebugDraw = false;

	/**
	 * Amplify macro Z drape so half-on-obstacle silhouette clearly slopes.
	 * Physics particles already differ; this only affects the visible shell.
	 */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0.8", ClampMax = "2.5"))
	float SkinMacroBoost = 1.75f;

	/** How much particle XY poke transfers to the shell (keep low — avoids ridged dents). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0", ClampMax = "1"))
	float SkinLateralScale = 0.15f;

	/**
	 * Gaussian sigma / body radius for the macro height field.
	 * ~0.45 preserves half-body contrast; ~0.6+ washes drape toward flat.
	 */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float SkinSigmaScale = 0.45f;

	/** 0 = 3D distance, 1 = XY-only (better for support-height drape). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Skin", meta = (ClampMin = "0", ClampMax = "1"))
	float SkinPlanarBias = 0.9f;

	/** Viscous pull toward formation XY (1/s). High = thick goo. */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float FormationCreepRate = 6.5f;

	/**
	 * How strongly formation pulls in Z (0 = free height / drape).
	 * Keep low so particles on a rock stay high while floor-side stay low.
	 */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity", meta = (ClampMin = "0", ClampMax = "1"))
	float FormationVerticalScale = 0.12f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float VelocityDamping = 14.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity")
	float NeighborSoftness = 0.35f;

	/** Neighbor distance correction scale on Z (low → preserve slope). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Viscosity", meta = (ClampMin = "0", ClampMax = "1"))
	float NeighborVerticalScale = 0.18f;

	/** Creep rate onto per-particle support height (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Support")
	float SupportSettleRate = 10.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume|Support")
	float SupportProbeExtraCm = 14.f;

	UPROPERTY(EditAnywhere, Category = "SoftVolume")
	float GroundNormalZ = 0.45f;

protected:
	struct FParticle
	{
		FVector RestLocal = FVector::ZeroVector; // relative to body center
		FVector WorldPos = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Radius = 20.f;
		float RestDistToCore = 0.f;
		/** Desired sphere-center when sitting on local support. */
		FVector SupportCenter = FVector::ZeroVector;
		bool bHasSupport = false;
	};

	void BuildDefaultLayout(float BodyRadiusCm);
	FVector BodyCenterFromActor(const AActor* Actor) const;
	bool SweepParticle(AActor* Ignore, const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const;
	void ProbeSupports(AActor* Ignore, bool& bOutGrounded);
	void ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter);
	void SettleToSupports(float Dt);
	void SyncActorFromParticles(ASlimePawn* Pawn);

	TArray<FParticle> Particles;
	FVector RestComLocal = FVector::ZeroVector;
	float BodyRadiusCm = 50.f;
	float BodyHeightCm = 52.f;
};
