#include "SlimeMovementComponent.h"
#include "ExpeditionDirector.h"
#include "SlimePawn.h"
#include "RaisingSlimeTypes.h"
#include "Components/SphereComponent.h"

USlimeMovementComponent::USlimeMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeMovementComponent::ResetMotion()
{
	PlanarVelocity = FVector::ZeroVector;
	VerticalVelocity = 0.f;
	bExtraJump = true;
	WallNormal = FVector::ZeroVector;
	WallFramesLeft = 0;
}

void USlimeMovementComponent::AddBounceBurst(ASlimePawn* Pawn, AExpeditionDirector* Director)
{
	if (!Pawn || !Director || Director->GetActiveTrait() != ETraitId::Bounce)
	{
		return;
	}

	const float BurstMps = Director->IsActiveConfirmed() ? 7.5f : 5.f;
	const FVector Forward = Pawn->GetActorForwardVector();
	PlanarVelocity += FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal() * (BurstMps * RaisingSlimeUnits::MetersToCm);
}

void USlimeMovementComponent::StepFixed(float Dt, ASlimePawn* Pawn, AExpeditionDirector* Director, const FVector2D& MoveInput, bool bMenu, bool bJumpQueued, bool& bOutJumpConsumed)
{
	if (!Pawn)
	{
		return;
	}

	const float Mass = Director ? Director->GetTotalMass() : 0.f;
	float Speed = FMath::Clamp(RaisingSlimeUnits::MoveSpeedMaxMps - Mass * 0.07f, 3.4f, RaisingSlimeUnits::MoveSpeedMaxMps);
	const bool bBounce = Director && Director->GetActiveTrait() == ETraitId::Bounce;
	const bool bConfirmed = Director && Director->IsActiveConfirmed();
	if (bBounce && bConfirmed)
	{
		Speed += 0.6f;
	}

	Integrate(Dt, Pawn, Speed, MoveInput, bMenu, bBounce, bConfirmed, bJumpQueued, bOutJumpConsumed);
}

void USlimeMovementComponent::Integrate(float Dt, ASlimePawn* Pawn, float SpeedMps, const FVector2D& MoveInput, bool bMenu, bool bBounce, bool bBounceConfirmed, bool bJumpQueued, bool& bOutJumpConsumed)
{
	const float Now = Pawn->GetWorld()->GetTimeSeconds();
	FVector2D Input = MoveInput;
	if (Input.SizeSquared() > 1.f)
	{
		Input.Normalize();
	}

	if (bMenu)
	{
		PlanarVelocity = FVector::ZeroVector;
	}
	else
	{
		const FRotator YawRot(0.f, Pawn->GetViewYaw(), 0.f);
		// Enhanced Input Axis2D: X = A/D (right), Y = W/S (forward)
		const FVector Desired = YawRot.RotateVector(FVector(Input.Y, Input.X, 0.f)) * (SpeedMps * RaisingSlimeUnits::MetersToCm);
		const float Rate = Input.SizeSquared() > 0.01f ? 16.f : RaisingSlimeUnits::MoveDecelMps2;
		const float MaxDelta = Rate * RaisingSlimeUnits::MetersToCm * Dt;
		const FVector Diff = Desired - PlanarVelocity;
		const float Dist = Diff.Size();
		if (Dist <= MaxDelta || Dist < KINDA_SMALL_NUMBER)
		{
			PlanarVelocity = Desired;
		}
		else
		{
			PlanarVelocity += Diff * (MaxDelta / Dist);
		}

		if (WallFramesLeft > 0 && WallNormal.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			FVector Wall = WallNormal;
			Wall.Z = 0.f;
			if (Wall.Normalize())
			{
				const float Into = FVector::DotProduct(PlanarVelocity, Wall);
				if (Into < 0.f)
				{
					PlanarVelocity -= Wall * Into;
				}
			}
			--WallFramesLeft;
		}
	}

	bOutJumpConsumed = false;
	if (bJumpQueued && !bMenu)
	{
		if (bGrounded)
		{
			VerticalVelocity = (bBounce && bBounceConfirmed ? 8.2f : RaisingSlimeUnits::JumpSpeedMps) * RaisingSlimeUnits::MetersToCm;
			bOutJumpConsumed = true;
		}
		else if (bBounce && bExtraJump)
		{
			bExtraJump = false;
			VerticalVelocity = (bBounceConfirmed ? 7.6f : 6.6f) * RaisingSlimeUnits::MetersToCm;
			bOutJumpConsumed = true;
		}
		else
		{
			bOutJumpConsumed = true;
		}
	}

	if (!bGrounded)
	{
		VerticalVelocity += RaisingSlimeUnits::SlimeGravityCm * Dt;
	}
	else if (VerticalVelocity < 0.f)
	{
		VerticalVelocity = 0.f;
	}

	if (bGrounded)
	{
		bExtraJump = true;
	}

	const FVector Delta(PlanarVelocity.X * Dt, PlanarVelocity.Y * Dt, VerticalVelocity * Dt);
	FHitResult Hit;
	const bool bBlocked = SweepMove(Pawn, Delta, Hit);
	if (bBlocked)
	{
		const FVector Normal = Hit.ImpactNormal;
		if (Normal.Z > 0.55f)
		{
			bGrounded = true;
			GroundedUntil = Now + 0.12f;
			if (VerticalVelocity < 0.f)
			{
				VerticalVelocity = 0.f;
			}
		}
		else
		{
			WallNormal = Normal;
			WallFramesLeft = 2;
			FVector Slide = FVector::VectorPlaneProject(Delta, Normal);
			FHitResult SlideHit;
			SweepMove(Pawn, Slide, SlideHit);
		}

		const FVector Remain = Delta * (1.f - Hit.Time);
		if (Remain.SizeSquared() > 1.f && Normal.Z <= 0.55f)
		{
			FHitResult Second;
			SweepMove(Pawn, FVector::VectorPlaneProject(Remain, Normal), Second);
		}
	}
	else
	{
		if (Now > GroundedUntil)
		{
			bGrounded = false;
		}
	}

	// Ground probe from sphere bottom
	if (USphereComponent* Sphere = Pawn->GetCollisionSphere())
	{
		const FVector Center = Sphere->GetComponentLocation();
		const float Radius = Sphere->GetScaledSphereRadius();
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeGround), false, Pawn);
		const FVector Start = Center;
		const FVector End = Center - FVector(0.f, 0.f, Radius + 8.f);
		if (Pawn->GetWorld()->SweepSingleByChannel(GroundHit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius * 0.98f), Params))
		{
			if (GroundHit.ImpactNormal.Z > 0.55f)
			{
				bGrounded = true;
				GroundedUntil = Now + 0.12f;
			}
		}
	}
}

bool USlimeMovementComponent::SweepMove(ASlimePawn* Pawn, const FVector& Delta, FHitResult& OutHit) const
{
	USphereComponent* Sphere = Pawn->GetCollisionSphere();
	if (!Sphere)
	{
		Pawn->AddActorWorldOffset(Delta, false);
		return false;
	}

	const FVector Start = Pawn->GetActorLocation();
	const FVector End = Start + Delta;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeMove), false, Pawn);
	const bool bHit = Pawn->GetWorld()->SweepSingleByChannel(
		OutHit,
		Sphere->GetComponentLocation(),
		Sphere->GetComponentLocation() + Delta,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()),
		Params);

	if (bHit)
	{
		const float Travel = FMath::Max(0.f, OutHit.Distance - 0.1f);
		const FVector Dir = Delta.GetSafeNormal();
		Pawn->SetActorLocation(Start + Dir * Travel, false);
		return true;
	}

	Pawn->SetActorLocation(End, false);
	return false;
}
