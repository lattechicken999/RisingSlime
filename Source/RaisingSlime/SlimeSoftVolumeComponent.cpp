#include "SlimeSoftVolumeComponent.h"
#include "SlimePawn.h"
#include "RaisingSlimeTypes.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSlimeSoftVolDebugDraw(
	TEXT("slime.SoftVolume.DebugDraw"),
	0,
	TEXT("Draw soft-volume particle spheres (0=off, 1=on). Also toggled by slime.SoftVolume.ToggleDebug."),
	ECVF_Cheat);

static FAutoConsoleCommand CCmdSlimeSoftVolToggleDebug(
	TEXT("slime.SoftVolume.ToggleDebug"),
	TEXT("Toggle soft-volume particle debug draw."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		const int32 Next = CVarSlimeSoftVolDebugDraw.GetValueOnGameThread() ? 0 : 1;
		CVarSlimeSoftVolDebugDraw->Set(Next, ECVF_SetByConsole);
		UE_LOG(LogTemp, Log, TEXT("slime.SoftVolume.DebugDraw = %d"), Next);
	}));

USlimeSoftVolumeComponent::USlimeSoftVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USlimeSoftVolumeComponent::BeginPlay()
{
	Super::BeginPlay();
	BodyHeightCm = RaisingSlimeUnits::BodyHeightCm;
	if (const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner()))
	{
		float R = 50.f;
		if (const USphereComponent* Sphere = Pawn->GetCollisionSphere())
		{
			R = Sphere->GetUnscaledSphereRadius();
		}
		RebuildRest(R);
	}
	else
	{
		RebuildRest(50.f);
	}
}

void USlimeSoftVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bDraw = bDebugDraw || (CVarSlimeSoftVolDebugDraw.GetValueOnGameThread() != 0);
	if (!bDraw || !bEnabled || Particles.Num() == 0 || !GetWorld())
	{
		return;
	}

	for (int32 I = 0; I < Particles.Num(); ++I)
	{
		const FParticle& P = Particles[I];
		const FColor Color = P.bHasSupport ? FColor(40, 220, 120) : FColor(80, 160, 255);
		DrawDebugSphere(GetWorld(), P.WorldPos, P.Radius, 10, Color, false, -1.f, 0, 1.2f);
		if (P.bHasSupport)
		{
			DrawDebugLine(GetWorld(), P.WorldPos, P.SupportCenter, FColor(255, 200, 40), false, -1.f, 0, 1.5f);
		}
	}
}

void USlimeSoftVolumeComponent::BuildDefaultLayout(float InBodyRadiusCm)
{
	Particles.Reset();
	BodyRadiusCm = FMath::Max(20.f, InBodyRadiusCm);

	auto Add = [this](const FVector& Local, float Rad)
	{
		FParticle P;
		P.RestLocal = Local;
		P.Radius = Rad;
		P.RestDistToCore = Local.Size();
		Particles.Add(P);
	};

	// Core + support under COM (floor contact).
	Add(FVector::ZeroVector, BodyRadiusCm * 0.32f);
	Add(FVector(0.f, 0.f, -BodyRadiusCm * 0.55f), BodyRadiusCm * 0.28f);

	// Bottom ring: intentional gaps so thin poles (~5–12cm) pass between spheres.
	{
		const int32 N = 6;
		const float RingR = BodyRadiusCm * 0.68f;
		const float Z = -BodyRadiusCm * 0.42f;
		const float Rad = BodyRadiusCm * 0.26f;
		for (int32 I = 0; I < N; ++I)
		{
			const float A = (2.f * PI * I) / N;
			Add(FVector(FMath::Cos(A) * RingR, FMath::Sin(A) * RingR, Z), Rad);
		}
	}

	// Equator ring — fill volume, still overlapping.
	{
		const int32 N = 8;
		const float RingR = BodyRadiusCm * 0.62f;
		const float Rad = BodyRadiusCm * 0.30f;
		for (int32 I = 0; I < N; ++I)
		{
			const float A = (2.f * PI * I) / N + PI / N;
			Add(FVector(FMath::Cos(A) * RingR, FMath::Sin(A) * RingR, 0.f), Rad);
		}
	}

	// Upper ring.
	{
		const int32 N = 6;
		const float RingR = BodyRadiusCm * 0.48f;
		const float Z = BodyRadiusCm * 0.40f;
		const float Rad = BodyRadiusCm * 0.28f;
		for (int32 I = 0; I < N; ++I)
		{
			const float A = (2.f * PI * I) / N;
			Add(FVector(FMath::Cos(A) * RingR, FMath::Sin(A) * RingR, Z), Rad);
		}
	}

	RestComLocal = FVector::ZeroVector;
	for (const FParticle& P : Particles)
	{
		RestComLocal += P.RestLocal;
	}
	RestComLocal /= FMath::Max(1, Particles.Num());
}

void USlimeSoftVolumeComponent::RebuildRest(float InBodyRadiusCm)
{
	BuildDefaultLayout(InBodyRadiusCm);
	ResetToActor();
}

void USlimeSoftVolumeComponent::EnsureRadius(float InBodyRadiusCm)
{
	if (Particles.Num() > 0 && FMath::Abs(InBodyRadiusCm - BodyRadiusCm) < 0.5f)
	{
		return;
	}
	RebuildRest(InBodyRadiusCm);
}

FVector USlimeSoftVolumeComponent::BodyCenterFromActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}
	return Actor->GetActorLocation() + FVector(0.f, 0.f, BodyHeightCm);
}

void USlimeSoftVolumeComponent::ResetToActor()
{
	AActor* Owner = GetOwner();
	if (!Owner || Particles.Num() == 0)
	{
		return;
	}
	const FQuat YawQ = FRotator(0.f, Owner->GetActorRotation().Yaw, 0.f).Quaternion();
	const FVector Center = BodyCenterFromActor(Owner);
	for (FParticle& P : Particles)
	{
		P.WorldPos = Center + YawQ.RotateVector(P.RestLocal);
		P.Velocity = FVector::ZeroVector;
		P.bHasSupport = false;
		P.SupportCenter = P.WorldPos;
	}
}

void USlimeSoftVolumeComponent::TeleportWithActor()
{
	ResetToActor();
}

int32 USlimeSoftVolumeComponent::GetSupportedParticleCount() const
{
	int32 N = 0;
	for (const FParticle& P : Particles)
	{
		if (P.bHasSupport)
		{
			++N;
		}
	}
	return N;
}

FVector USlimeSoftVolumeComponent::GetParticleWorldPos(int32 Index) const
{
	return Particles.IsValidIndex(Index) ? Particles[Index].WorldPos : FVector::ZeroVector;
}

float USlimeSoftVolumeComponent::GetParticleRadius(int32 Index) const
{
	return Particles.IsValidIndex(Index) ? Particles[Index].Radius : 0.f;
}

FVector USlimeSoftVolumeComponent::GetBodyCenterWorld() const
{
	if (Particles.Num() == 0)
	{
		return BodyCenterFromActor(GetOwner());
	}
	FVector Sum = FVector::ZeroVector;
	for (const FParticle& P : Particles)
	{
		Sum += P.WorldPos;
	}
	return Sum / Particles.Num();
}

bool USlimeSoftVolumeComponent::SweepParticle(AActor* Ignore, const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeSoftVol), false, Ignore);
	return World->SweepSingleByChannel(
		OutHit, Start, End, FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeSphere(Radius), Params);
}

void USlimeSoftVolumeComponent::ProbeSupports(AActor* Ignore, bool& bOutGrounded)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeSoftSupport), false, Ignore);
	for (FParticle& P : Particles)
	{
		P.bHasSupport = false;
		// Start slightly above so we still find a surface when already sitting on it.
		const FVector Start = P.WorldPos + FVector(0.f, 0.f, FMath::Max(2.f, P.Radius * 0.15f));
		const FVector End = P.WorldPos - FVector(0.f, 0.f, P.Radius + SupportProbeExtraCm);
		FHitResult Hit;
		if (!World->SweepSingleByChannel(
			Hit, Start, End, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeSphere(P.Radius * 0.92f), Params))
		{
			continue;
		}
		if (Hit.ImpactNormal.Z < GroundNormalZ)
		{
			// Wall / thin-pole side — not a sit surface (keep pierce/settle).
			continue;
		}

		// Sphere center that rests on this surface.
		P.SupportCenter = Hit.ImpactPoint + Hit.ImpactNormal * P.Radius;
		P.bHasSupport = true;
		bOutGrounded = true;
	}
}

void USlimeSoftVolumeComponent::ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter)
{
	const float Creep = 1.f - FMath::Exp(-FMath::Max(0.1f, FormationCreepRate) * Dt);
	const float Damp = FMath::Exp(-FMath::Max(0.1f, VelocityDamping) * Dt);
	const float VertScale = FMath::Clamp(FormationVerticalScale, 0.f, 1.f);

	for (FParticle& P : Particles)
	{
		const FVector Formation = BodyCenter + YawQ.RotateVector(P.RestLocal - RestComLocal);
		FVector Target = Formation;

		// Supported particles: XY may ooze toward formation, Z locked to local support.
		if (P.bHasSupport)
		{
			Target.X = Formation.X;
			Target.Y = Formation.Y;
			Target.Z = P.SupportCenter.Z;
		}
		else
		{
			// Free particles: strong XY formation, weak Z (allows drape / hang).
			Target.Z = FMath::Lerp(P.WorldPos.Z, Formation.Z, VertScale);
		}

		FVector ToTarget = Target - P.WorldPos;
		// Soften vertical pull further when unsupported so COM rise doesn't lift floor side.
		if (!P.bHasSupport)
		{
			ToTarget.Z *= VertScale;
		}

		P.Velocity = FMath::Lerp(P.Velocity, ToTarget * FormationCreepRate, Creep);
		P.Velocity *= Damp;
		P.WorldPos += P.Velocity * Dt;

		FVector BlendTarget = Target;
		if (!P.bHasSupport)
		{
			BlendTarget.Z = P.WorldPos.Z; // don't snap Z via lerp
		}
		P.WorldPos = FMath::Lerp(P.WorldPos, BlendTarget, Creep * 0.55f);
		if (P.bHasSupport)
		{
			// Keep Z glued toward support after XY blend.
			P.WorldPos.Z = FMath::Lerp(P.WorldPos.Z, P.SupportCenter.Z, Creep * 0.85f);
		}
	}

	// Soft neighbor distance — XY strong, Z weak so slopes survive.
	const float Soft = NeighborSoftness * Creep;
	const float VertN = FMath::Clamp(NeighborVerticalScale, 0.f, 1.f);
	if (Soft > KINDA_SMALL_NUMBER)
	{
		for (int32 I = 0; I < Particles.Num(); ++I)
		{
			for (int32 J = I + 1; J < Particles.Num(); ++J)
			{
				const float RestDist = FVector::Dist(Particles[I].RestLocal, Particles[J].RestLocal);
				FVector Delta = Particles[J].WorldPos - Particles[I].WorldPos;
				const float Cur = Delta.Size();
				if (Cur < KINDA_SMALL_NUMBER || RestDist < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				if (RestDist > (Particles[I].Radius + Particles[J].Radius) * 1.35f)
				{
					continue;
				}
				const FVector N = Delta / Cur;
				const float Err = Cur - RestDist;
				FVector Corr = N * (Err * Soft * 0.5f);
				Corr.Z *= VertN;
				Particles[I].WorldPos += Corr;
				Particles[J].WorldPos -= Corr;
			}
		}
	}
}

void USlimeSoftVolumeComponent::SettleToSupports(float Dt)
{
	const float Alpha = 1.f - FMath::Exp(-FMath::Max(0.1f, SupportSettleRate) * Dt);
	for (FParticle& P : Particles)
	{
		if (!P.bHasSupport)
		{
			continue;
		}
		// Height from local support; light XY stick so we don't slide off slopes instantly.
		P.WorldPos.Z = FMath::Lerp(P.WorldPos.Z, P.SupportCenter.Z, Alpha);
		P.WorldPos.X = FMath::Lerp(P.WorldPos.X, P.SupportCenter.X, Alpha * 0.2f);
		P.WorldPos.Y = FMath::Lerp(P.WorldPos.Y, P.SupportCenter.Y, Alpha * 0.2f);
		if (P.Velocity.Z < 0.f)
		{
			P.Velocity.Z = 0.f;
		}
		P.Velocity *= (1.f - Alpha * 0.5f);
	}
}

void USlimeSoftVolumeComponent::SyncActorFromParticles(ASlimePawn* Pawn)
{
	if (!Pawn || Particles.Num() == 0)
	{
		return;
	}

	const FQuat YawQ = FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Quaternion();
	FVector Com = FVector::ZeroVector;
	for (const FParticle& P : Particles)
	{
		Com += P.WorldPos;
	}
	Com /= Particles.Num();

	// Actor feet = body center - BodyHeight; body center tracks particle COM.
	const FVector BodyCenter = Com - YawQ.RotateVector(RestComLocal);
	const FVector NewActor = BodyCenter - FVector(0.f, 0.f, BodyHeightCm);
	Pawn->SetActorLocation(NewActor, false);
}

FVector USlimeSoftVolumeComponent::SampleDisplacementLocal(const FVector& MeshLocalPoint) const
{
	if (Particles.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	const AActor* Owner = GetOwner();
	const FQuat YawQ = FRotator(0.f, Owner ? Owner->GetActorRotation().Yaw : 0.f, 0.f).Quaternion();
	const FQuat InvYaw = YawQ.Inverse();
	// Rest frame = actor body center (COM-synced). Particle Z deltas vs this frame ARE the drape.
	const FVector Center = BodyCenterFromActor(Owner);

	// Macro support-height field: wide Gaussian on planar distance (smooth, no IDW spikes).
	// Sigma kept moderate so half-up / half-down contrast survives; SkinMacroBoost restores read.
	const float PlanarBias = FMath::Clamp(SkinPlanarBias, 0.f, 1.f);
	const float MacroBoost = FMath::Clamp(SkinMacroBoost, 0.8f, 2.5f);
	const float LatScale = FMath::Clamp(SkinLateralScale, 0.f, 1.f);
	const float Sigma = FMath::Max(16.f, BodyRadiusCm * FMath::Clamp(SkinSigmaScale, 0.25f, 1.f));
	const float InvTwoSig2 = 1.f / (2.f * Sigma * Sigma);

	FVector Acc = FVector::ZeroVector;
	float WSum = 0.f;
	for (const FParticle& P : Particles)
	{
		const FVector RestLocalCentered = P.RestLocal - RestComLocal;
		const FVector RestWorld = Center + YawQ.RotateVector(RestLocalCentered);
		const FVector DispLocal = InvYaw.RotateVector(P.WorldPos - RestWorld);

		const float Dist3 = FVector::Dist(MeshLocalPoint, RestLocalCentered);
		const float DistXY = FVector::Dist2D(MeshLocalPoint, RestLocalCentered);
		const float Dist = FMath::Lerp(Dist3, DistXY, PlanarBias);
		const float W = FMath::Exp(-(Dist * Dist) * InvTwoSig2);
		Acc += DispLocal * W;
		WSum += W;
	}
	if (WSum < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	FVector Out = Acc / WSum;
	Out.X *= LatScale;
	Out.Y *= LatScale;
	Out.Z *= MacroBoost;
	return Out;
}

bool USlimeSoftVolumeComponent::StepMove(ASlimePawn* Pawn, const FVector& DesiredDelta, float Dt, bool& bOutGrounded)
{
	bOutGrounded = false;
	if (!bEnabled || !Pawn || Particles.Num() == 0)
	{
		return false;
	}

	const FQuat YawQ = FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Quaternion();

	// 1) Each particle tries DesiredDelta independently — thin poles only block local spheres.
	for (FParticle& P : Particles)
	{
		const FVector Start = P.WorldPos;
		const FVector End = Start + DesiredDelta;
		FHitResult Hit;
		if (SweepParticle(Pawn, Start, End, P.Radius, Hit))
		{
			if (Hit.bStartPenetrating)
			{
				const float Push = FMath::Abs(Hit.PenetrationDepth) + 1.5f;
				P.WorldPos = Start + Hit.Normal * Push;
				P.Velocity = FVector::VectorPlaneProject(P.Velocity, Hit.Normal);
			}
			else
			{
				const float Travel = FMath::Max(0.f, Hit.Distance - 0.35f);
				const FVector Dir = DesiredDelta.GetSafeNormal();
				P.WorldPos = Start + Dir * Travel;

				const FVector Remain = DesiredDelta * (1.f - Hit.Time);
				FVector Slide = FVector::VectorPlaneProject(Remain, Hit.ImpactNormal);
				if (Slide.SizeSquared() > 1.f)
				{
					FHitResult SlideHit;
					if (SweepParticle(Pawn, P.WorldPos, P.WorldPos + Slide, P.Radius, SlideHit))
					{
						if (!SlideHit.bStartPenetrating)
						{
							const float STravel = FMath::Max(0.f, SlideHit.Distance - 0.25f);
							P.WorldPos += Slide.GetSafeNormal() * STravel;
						}
					}
					else
					{
						P.WorldPos += Slide;
					}
				}
			}
		}
		else
		{
			P.WorldPos = End;
		}
	}

	// 2) Per-particle support heights (rock vs floor) — enables edge drape.
	ProbeSupports(Pawn, bOutGrounded);

	// 3) Viscous formation: XY together, Z mostly free / support-driven.
	FVector Com = GetBodyCenterWorld();
	// Planar-biased COM for formation so a raised rock side doesn't yank floor XY as hard.
	{
		FVector PlanarCom = FVector::ZeroVector;
		int32 N = 0;
		for (const FParticle& P : Particles)
		{
			PlanarCom += P.WorldPos;
			++N;
		}
		if (N > 0)
		{
			Com = PlanarCom / N;
		}
	}
	ApplyViscosity(Dt, YawQ, Com);

	// 4) Creep onto local supports (viscous, no bounce).
	SettleToSupports(Dt);

	// 5) Re-resolve after viscosity (particles may have crept into geometry).
	for (FParticle& P : Particles)
	{
		FHitResult Hit;
		const FVector ProbeEnd = P.WorldPos + FVector(0.f, 0.f, 0.5f);
		if (SweepParticle(Pawn, P.WorldPos, ProbeEnd, P.Radius, Hit) && Hit.bStartPenetrating)
		{
			P.WorldPos += Hit.Normal * (FMath::Abs(Hit.PenetrationDepth) + 1.5f);
			if (Hit.Normal.Z > GroundNormalZ)
			{
				bOutGrounded = true;
			}
		}
	}

	// 6) Refresh supports after depen / settle so grounded flag stays accurate.
	ProbeSupports(Pawn, bOutGrounded);

	// 7) Actor follows particle COM (mid-height when half-on rock — particles keep local Z).
	SyncActorFromParticles(Pawn);
	return true;
}
