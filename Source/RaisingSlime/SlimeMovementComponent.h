#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeMovementComponent.generated.h"

class ASlimePawn;
class AExpeditionDirector;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeMovementComponent();

	void StepFixed(float Dt, ASlimePawn* Pawn, AExpeditionDirector* Director, const FVector2D& MoveInput, bool bMenu, bool bJumpQueued, bool& bOutJumpConsumed);
	void AddBounceBurst(ASlimePawn* Pawn, AExpeditionDirector* Director);

	FVector GetPlanarVelocity() const { return PlanarVelocity; }
	float GetVerticalVelocity() const { return VerticalVelocity; }
	bool IsGrounded() const { return bGrounded; }
	bool IsAscending() const { return VerticalVelocity > 150.f; }

	/** World-space planar move intent (desired velocity direction × weight). */
	FVector GetMoveIntent() const { return MoveIntent; }

	/** Centimeters lifted by step-up assist this fixed step (for jelly flow). */
	float GetLastStepUpCm() const { return LastStepUpCm; }

	/**
	 * Max climbable ledge as a fraction of scaled collision radius.
	 * Default 0.64 → ~32cm at base radius 50cm.
	 */
	UPROPERTY(EditAnywhere, Category = "Movement|Step")
	float StepHeightFraction = 0.64f;

	UPROPERTY(EditAnywhere, Category = "Movement|Step")
	float MinStepHeightCm = 6.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Step")
	float MaxStepHeightCapCm = 48.f;

	/**
	 * Whole-actor step-up (hard path only). Default off — soft-volume creep climb
	 * is the primary way onto low/mid obstacles (no hop).
	 */
	UPROPERTY(EditAnywhere, Category = "Movement|Step")
	bool bEnableStepUp = false;

	/**
	 * Jump feel: snappy apex (less hang) while keeping viscous slime weight.
	 * Stronger fall / apex pull preferred over a tiny hop.
	 */
	UPROPERTY(EditAnywhere, Category = "Movement|Jump", meta = (ClampMin = "4", ClampMax = "12"))
	float JumpSpeedMps = 7.8f;

	/** Extra gravity scale while falling (Vz < 0). */
	UPROPERTY(EditAnywhere, Category = "Movement|Jump", meta = (ClampMin = "1", ClampMax = "2.5"))
	float JumpFallGravityScale = 1.55f;

	/**
	 * When rising and |Vz| is below this (cm/s), blend toward JumpApexGravityScale
	 * so the top of the arc does not float.
	 */
	UPROPERTY(EditAnywhere, Category = "Movement|Jump", meta = (ClampMin = "50", ClampMax = "800"))
	float JumpApexVelocityCm = 280.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Jump", meta = (ClampMin = "1", ClampMax = "2.5"))
	float JumpApexGravityScale = 1.7f;

	void ResetMotion();
	void SetPlanarVelocity(const FVector& V) { PlanarVelocity = V; }

protected:
	void Integrate(float Dt, ASlimePawn* Pawn, float SpeedMps, const FVector2D& MoveInput, bool bMenu, bool bBounce, bool bBounceConfirmed, bool bJumpQueued, bool& bOutJumpConsumed);
	bool IntegrateSoftVolume(float Dt, ASlimePawn* Pawn, const FVector& Delta, float Now);
	bool SweepMove(ASlimePawn* Pawn, const FVector& Delta, FHitResult& OutHit) const;
	bool TryStepUp(ASlimePawn* Pawn, const FVector& DesiredDelta, const FHitResult& BlockHit);
	float GetMaxStepHeightCm(const ASlimePawn* Pawn) const;

	FVector PlanarVelocity = FVector::ZeroVector;
	FVector MoveIntent = FVector::ZeroVector;
	float VerticalVelocity = 0.f;
	float LastStepUpCm = 0.f;
	bool bGrounded = false;
	bool bExtraJump = true;
	FVector WallNormal = FVector::ZeroVector;
	float GroundedUntil = 0.f;
	int32 WallFramesLeft = 0;
};
