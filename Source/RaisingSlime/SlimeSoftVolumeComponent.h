#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeSoftVolumeComponent.generated.h"

class ASlimePawn;
class AActor;

/**
 * Soft-volume collision: overlapping spheres that resolve WorldStatic individually.
 * During Held devour, particles climb-wrap prey (ferrofluid) bottom→top with viscous lag.
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

	/** Register Held prey for ferrofluid climb-wrap (bounds from actor). */
	void SetEngulfClimbTarget(AActor* Prey);
	void ClearEngulfClimbTarget();
	bool HasEngulfClimbTarget() const { return Climb.bActive; }
	/** 0 = base contact only, 1 = wrapped to prey top. */
	float GetEngulfClimbProgress01() const { return Climb.bActive ? Climb.ClimbFront01 : 0.f; }

	/** Jelly PMC climb skin reads this sleeve (world space). */
	bool TryGetEngulfClimbSleeve(FVector& OutCenter, float& OutRadiusXY, float& OutBottomZ, float& OutTopZ, float& OutFront01) const;

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
	float EngulfSkinLateralBoost = 1.f;

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

	/** How fast climb front rises while pressing into prey (1/s toward full height). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float EngulfClimbAdvanceRate = 1.85f;

	/** Viscous follow onto prey surface sleeve (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0.5", ClampMax = "20"))
	float EngulfClimbCreepRate = 10.f;

	/** Climb front decays when player backs off (1/s). */
	UPROPERTY(EditAnywhere, Category = "SoftVolume|Engulf", meta = (ClampMin = "0", ClampMax = "2"))
	float EngulfClimbDecayRate = 0.2f;

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
	void ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter);
	void SettleToSupports(float Dt);
	void ApplyEngulfClimb(ASlimePawn* Pawn, float Dt, const FQuat& YawQ);
	void SyncActorFromParticles(ASlimePawn* Pawn);
	void RefreshClimbBoundsFromPrey();

	TArray<FParticle> Particles;
	FEngulfClimb Climb;
	FVector RestComLocal = FVector::ZeroVector;
	float BodyRadiusCm = 50.f;
	float BodyHeightCm = 52.f;
};
