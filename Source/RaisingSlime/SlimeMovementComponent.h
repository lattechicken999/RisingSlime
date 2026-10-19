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

	void ResetMotion();
	void SetPlanarVelocity(const FVector& V) { PlanarVelocity = V; }

protected:
	void Integrate(float Dt, ASlimePawn* Pawn, float SpeedMps, const FVector2D& MoveInput, bool bMenu, bool bBounce, bool bBounceConfirmed, bool bJumpQueued, bool& bOutJumpConsumed);
	bool SweepMove(ASlimePawn* Pawn, const FVector& Delta, FHitResult& OutHit) const;

	FVector PlanarVelocity = FVector::ZeroVector;
	float VerticalVelocity = 0.f;
	bool bGrounded = false;
	bool bExtraJump = true;
	FVector WallNormal = FVector::ZeroVector;
	float GroundedUntil = 0.f;
	int32 WallFramesLeft = 0;
};
