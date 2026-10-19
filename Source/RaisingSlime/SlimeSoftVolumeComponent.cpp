#include "SlimeSoftVolumeComponent.h"
#include "SlimePawn.h"
#include "RaisingSlimeTypes.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

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
	if (!bDebugDraw || !bEnabled || Particles.Num() == 0 || !GetWorld())
	{
		return;
	}

	for (int32 I = 0; I < Particles.Num(); ++I)
	{
		const FParticle& P = Particles[I];
		DrawDebugSphere(GetWorld(), P.WorldPos, P.Radius, 10, FColor(40, 220, 120), false, -1.f, 0, 1.2f);
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
		const float Rad = BodyRadiusCm * 0.26f; // gap ≈ chord - 2r > ~7cm at R=50
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
	}
}

void USlimeSoftVolumeComponent::TeleportWithActor()
{
	ResetToActor();
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

void USlimeSoftVolumeComponent::ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter)
{
	const float Creep = 1.f - FMath::Exp(-FMath::Max(0.1f, FormationCreepRate) * Dt);
	const float Damp = FMath::Exp(-FMath::Max(0.1f, VelocityDamping) * Dt);

	for (FParticle& P : Particles)
	{
		const FVector Target = BodyCenter + YawQ.RotateVector(P.RestLocal - RestComLocal);
		const FVector ToTarget = Target - P.WorldPos;
		// Overdamped: velocity toward formation, then hard kill — no spring bounce.
		P.Velocity = FMath::Lerp(P.Velocity, ToTarget * FormationCreepRate, Creep);
		P.Velocity *= Damp;
		P.WorldPos += P.Velocity * Dt;
		P.WorldPos = FMath::Lerp(P.WorldPos, Target, Creep * 0.55f);
	}

	// Soft neighbor distance constraints (viscous, not oscillatory).
	const float Soft = NeighborSoftness * Creep;
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
				// Only constrain pairs that were neighbors in rest.
				if (RestDist > (Particles[I].Radius + Particles[J].Radius) * 1.35f)
				{
					continue;
				}
				const FVector N = Delta / Cur;
				const float Err = Cur - RestDist;
				const FVector Corr = N * (Err * Soft * 0.5f);
				Particles[I].WorldPos += Corr;
				Particles[J].WorldPos -= Corr;
			}
		}
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
	const FVector Center = GetBodyCenterWorld();

	FVector Acc = FVector::ZeroVector;
	float WSum = 0.f;
	for (const FParticle& P : Particles)
	{
		const FVector RestWorld = Center + YawQ.RotateVector(P.RestLocal - RestComLocal);
		const FVector DispWorld = P.WorldPos - RestWorld;
		const FVector RestLocalCentered = P.RestLocal - RestComLocal;
		const float Dist = FVector::Dist(MeshLocalPoint, RestLocalCentered);
		const float Sigma = FMath::Max(8.f, P.Radius * 1.6f);
		const float W = FMath::Exp(-(Dist * Dist) / (2.f * Sigma * Sigma));
		Acc += InvYaw.RotateVector(DispWorld) * W;
		WSum += W;
	}
	if (WSum < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	return Acc / WSum;
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

				// Slide leftover along the hit plane (viscous slide, no bounce).
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

				if (Hit.ImpactNormal.Z > GroundNormalZ)
				{
					bOutGrounded = true;
				}
			}
		}
		else
		{
			P.WorldPos = End;
		}
	}

	// 2) Viscous formation around current COM (ooze back together).
	FVector Com = GetBodyCenterWorld();
	ApplyViscosity(Dt, YawQ, Com);

	// 3) Re-resolve after viscosity (particles may have crept into geometry).
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

	// 4) Ground probe per lower particle (supports when only some spheres sit on floor).
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeSoftGround), false, Pawn);
		for (const FParticle& P : Particles)
		{
			if (P.RestLocal.Z > -BodyRadiusCm * 0.15f)
			{
				continue;
			}
			FHitResult GHit;
			const FVector End = P.WorldPos - FVector(0.f, 0.f, P.Radius + 10.f);
			if (GetWorld() && GetWorld()->SweepSingleByChannel(
				GHit, P.WorldPos, End, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(P.Radius * 0.92f), Params))
			{
				if (GHit.ImpactNormal.Z > GroundNormalZ)
				{
					bOutGrounded = true;
					break;
				}
			}
		}
	}

	// 5) Actor follows particle COM — if bottom spheres settle beside a pole, whole body drops.
	SyncActorFromParticles(Pawn);
	return true;
}
