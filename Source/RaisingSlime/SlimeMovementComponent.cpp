#include "SlimeMovementComponent.h"
#include "ExpeditionDirector.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "RaisingSlimeTypes.h"
#include "Components/SphereComponent.h"

USlimeMovementComponent::USlimeMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeMovementComponent::ResetMotion()
{
	PlanarVelocity = FVector::ZeroVector;
	MoveIntent = FVector::ZeroVector;
	VerticalVelocity = 0.f;
	LastStepUpCm = 0.f;
	bExtraJump = true;
	WallNormal = FVector::ZeroVector;
	WallFramesLeft = 0;
}

float USlimeMovementComponent::GetMaxStepHeightCm(const ASlimePawn* Pawn) const
{
	float Radius = 50.f;
	if (Pawn)
	{
		if (const USphereComponent* Sphere = Pawn->GetCollisionSphere())
		{
			Radius = Sphere->GetScaledSphereRadius();
		}
	}
	return FMath::Clamp(Radius * StepHeightFraction, MinStepHeightCm, MaxStepHeightCapCm);
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

	LastStepUpCm = 0.f;

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

bool USlimeMovementComponent::TryStepUp(ASlimePawn* Pawn, const FVector& DesiredDelta, const FHitResult& BlockHit)
{
	if (!bEnableStepUp || !Pawn)
	{
		return false;
	}

	const FVector HitNormal = BlockHit.bStartPenetrating ? BlockHit.Normal : BlockHit.ImpactNormal;
	// Only climb near-vertical faces / short ledges (not floors/ceilings).
	if (HitNormal.Z > 0.45f || HitNormal.Z < -0.35f)
	{
		return false;
	}

	FVector Horiz = FVector(DesiredDelta.X, DesiredDelta.Y, 0.f);
	if (Horiz.SizeSquared() < 4.f)
	{
		return false;
	}
	// Must be moving somewhat into the obstacle.
	if (FVector::DotProduct(Horiz.GetSafeNormal(), HitNormal) > -0.05f)
	{
		return false;
	}

	USphereComponent* Sphere = Pawn->GetCollisionSphere();
	if (!Sphere)
	{
		return false;
	}

	const float MaxStep = GetMaxStepHeightCm(Pawn);
	const FVector ActorStart = Pawn->GetActorLocation();
	const FVector SphereStart = Sphere->GetComponentLocation();

	// 1) Lift up to MaxStep.
	FHitResult UpHit;
	const FVector UpDelta(0.f, 0.f, MaxStep);
	const bool bUpBlocked = SweepMove(Pawn, UpDelta, UpHit);
	float LiftCm = MaxStep;
	if (bUpBlocked)
	{
		if (UpHit.bStartPenetrating)
		{
			Pawn->SetActorLocation(ActorStart, false);
			return false;
		}
		LiftCm = FMath::Max(0.f, UpHit.Distance - 0.5f);
		if (LiftCm < MinStepHeightCm)
		{
			Pawn->SetActorLocation(ActorStart, false);
			return false;
		}
		// SweepMove already placed us partway up; ensure exact lift from start.
		Pawn->SetActorLocation(ActorStart + FVector(0.f, 0.f, LiftCm), false);
	}

	// 2) From elevated pose, push forward remaining horizontal (plus a bit past the lip).
	const FVector ForwardDelta = Horiz.GetSafeNormal() * FMath::Max(Horiz.Size(), Sphere->GetScaledSphereRadius() * 0.35f);
	FHitResult FwdHit;
	const bool bFwdBlocked = SweepMove(Pawn, ForwardDelta, FwdHit);
	if (bFwdBlocked && FwdHit.bStartPenetrating)
	{
		Pawn->SetActorLocation(ActorStart, false);
		return false;
	}
	if (bFwdBlocked && FwdHit.Distance < 2.f && FwdHit.ImpactNormal.Z < 0.5f)
	{
		// Still jammed into a tall wall — abort step.
		Pawn->SetActorLocation(ActorStart, false);
		return false;
	}

	// 3) Settle down onto the ledge so we don't float.
	FHitResult DownHit;
	const FVector DownDelta(0.f, 0.f, -LiftCm - 4.f);
	SweepMove(Pawn, DownDelta, DownHit);
	if (DownHit.bBlockingHit && DownHit.ImpactNormal.Z > 0.45f)
	{
		bGrounded = true;
	}

	LastStepUpCm = LiftCm;
	// Clear sticky wall clip for a few frames after a successful climb.
	WallFramesLeft = 0;
	WallNormal = FVector::ZeroVector;
	return true;
}

void USlimeMovementComponent::Integrate(float Dt, ASlimePawn* Pawn, float SpeedMps, const FVector2D& MoveInput, bool bMenu, bool bBounce, bool bBounceConfirmed, bool bJumpQueued, bool& bOutJumpConsumed)
{
	const float Now = Pawn->GetWorld()->GetTimeSeconds();
	FVector2D Input = MoveInput;
	if (Input.SizeSquared() > 1.f)
	{
		Input.Normalize();
	}

	MoveIntent = FVector::ZeroVector;
	if (bMenu)
	{
		PlanarVelocity = FVector::ZeroVector;
	}
	else
	{
		const FRotator YawRot(0.f, Pawn->GetViewYaw(), 0.f);
		// Enhanced Input Axis2D: X = A/D (right), Y = W/S (forward)
		const FVector Desired = YawRot.RotateVector(FVector(Input.Y, Input.X, 0.f)) * (SpeedMps * RaisingSlimeUnits::MetersToCm);
		MoveIntent = Desired;
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
			const float TakeoffMps = bBounce && bBounceConfirmed
				? FMath::Max(JumpSpeedMps + 0.6f, 8.4f)
				: JumpSpeedMps;
			VerticalVelocity = TakeoffMps * RaisingSlimeUnits::MetersToCm;
			bOutJumpConsumed = true;
		}
		else if (bBounce && bExtraJump)
		{
			bExtraJump = false;
			const float AirJumpMps = bBounceConfirmed ? (JumpSpeedMps - 0.2f) : (JumpSpeedMps - 1.2f);
			VerticalVelocity = FMath::Max(5.5f, AirJumpMps) * RaisingSlimeUnits::MetersToCm;
			bOutJumpConsumed = true;
		}
		else
		{
			bOutJumpConsumed = true;
		}
	}

	if (!bGrounded)
	{
		// Asymmetric gravity: punchy rise, short apex hang, heavier fall (viscous slam — not Mario float).
		float GravityScale = 1.f;
		if (VerticalVelocity < 0.f)
		{
			GravityScale = JumpFallGravityScale;
		}
		else if (JumpApexVelocityCm > 1.f && VerticalVelocity < JumpApexVelocityCm)
		{
			const float Apex01 = 1.f - (VerticalVelocity / JumpApexVelocityCm);
			GravityScale = FMath::Lerp(1.f, JumpApexGravityScale, Apex01);
		}
		VerticalVelocity += RaisingSlimeUnits::SlimeGravityCm * GravityScale * Dt;
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

	// Soft-volume path: per-particle WorldStatic — thin poles sit under/into body.
	if (IntegrateSoftVolume(Dt, Pawn, Delta, Now))
	{
		return;
	}

	FHitResult Hit;
	const bool bBlocked = SweepMove(Pawn, Delta, Hit);
	if (bBlocked)
	{
		const FVector Normal = Hit.bStartPenetrating ? Hit.Normal : Hit.ImpactNormal;
		if (Normal.Z > 0.55f)
		{
			bGrounded = true;
			GroundedUntil = Now + 0.12f;
			if (VerticalVelocity < 0.f)
			{
				VerticalVelocity = 0.f;
			}
			if (Hit.bStartPenetrating)
			{
				Pawn->AddActorWorldOffset(FVector(0.f, 0.f, FMath::Max(2.f, -Hit.PenetrationDepth + 2.f)), false);
			}

			// Continue leftover horizontal after floor hit.
			const FVector Remain(Delta.X * (1.f - Hit.Time), Delta.Y * (1.f - Hit.Time), 0.f);
			if (Remain.SizeSquared() > 1.f)
			{
				FHitResult Second;
				SweepMove(Pawn, Remain, Second);
			}
		}
		else
		{
			// Low obstacle: try step-up before sticky wall slide.
			const bool bStepped = TryStepUp(Pawn, Delta, Hit);
			if (!bStepped)
			{
				WallNormal = Normal;
				WallFramesLeft = 2;
				FVector Slide = FVector::VectorPlaneProject(Delta, Normal);
				FHitResult SlideHit;
				SweepMove(Pawn, Slide, SlideHit);

				const FVector Remain = Delta * (1.f - Hit.Time);
				if (Remain.SizeSquared() > 1.f)
				{
					FHitResult Second;
					SweepMove(Pawn, FVector::VectorPlaneProject(Remain, Normal), Second);
				}
			}
			else
			{
				bGrounded = true;
				GroundedUntil = Now + 0.18f;
				if (VerticalVelocity < 0.f)
				{
					VerticalVelocity = 0.f;
				}
			}
		}
	}
	else
	{
		if (Now > GroundedUntil)
		{
			bGrounded = false;
		}
	}

	// Ground probe against WorldStatic (floors), not ECC_Pawn (can miss greybox).
	if (USphereComponent* Sphere = Pawn->GetCollisionSphere())
	{
		const FVector Center = Sphere->GetComponentLocation();
		const float Radius = Sphere->GetScaledSphereRadius();
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeGround), false, Pawn);
		const FVector Start = Center;
		const FVector End = Center - FVector(0.f, 0.f, Radius + 12.f);
		if (Pawn->GetWorld()->SweepSingleByChannel(GroundHit, Start, End, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Radius * 0.95f), Params))
		{
			if (GroundHit.ImpactNormal.Z > 0.55f)
			{
				bGrounded = true;
				GroundedUntil = Now + 0.12f;
			}
		}
	}
}

bool USlimeMovementComponent::IntegrateSoftVolume(float Dt, ASlimePawn* Pawn, const FVector& Delta, float Now)
{
	USlimeSoftVolumeComponent* Soft = Pawn ? Pawn->GetSoftVolume() : nullptr;
	if (!Soft || !Soft->bEnabled)
	{
		return false;
	}

	bool bSoftGrounded = false;
	Soft->StepMove(Pawn, Delta, Dt, bSoftGrounded);
	if (bSoftGrounded)
	{
		bGrounded = true;
		GroundedUntil = Now + 0.14f;
		if (VerticalVelocity < 0.f)
		{
			VerticalVelocity = 0.f;
		}
		WallFramesLeft = 0;
		WallNormal = FVector::ZeroVector;
	}
	else if (Now > GroundedUntil)
	{
		bGrounded = false;
	}
	return true;
}

bool USlimeMovementComponent::SweepMove(ASlimePawn* Pawn, const FVector& Delta, FHitResult& OutHit) const
{
	USphereComponent* Sphere = Pawn->GetCollisionSphere();
	if (!Sphere)
	{
		Pawn->AddActorWorldOffset(Delta, false);
		return false;
	}

	const FVector ActorStart = Pawn->GetActorLocation();
	const FVector SphereStart = Sphere->GetComponentLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeMove), false, Pawn);
	const bool bHit = Pawn->GetWorld()->SweepSingleByChannel(
		OutHit,
		SphereStart,
		SphereStart + Delta,
		FQuat::Identity,
		ECC_WorldStatic,
		FCollisionShape::MakeSphere(Sphere->GetScaledSphereRadius()),
		Params);

	if (bHit)
	{
		if (OutHit.bStartPenetrating)
		{
			const FVector Push = OutHit.Normal * (FMath::Abs(OutHit.PenetrationDepth) + 2.f);
			Pawn->SetActorLocation(ActorStart + Push, false);
			return true;
		}
		const float Travel = FMath::Max(0.f, OutHit.Distance - 0.25f);
		const FVector Dir = Delta.GetSafeNormal();
		Pawn->SetActorLocation(ActorStart + Dir * Travel, false);
		return true;
	}

	Pawn->SetActorLocation(ActorStart + Delta, false);
	return false;
}
