#include "SlimeSoftVolumeComponent.h"
#include "SlimePawn.h"
#include "Devourable.h"
#include "RaisingSlimeTypes.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
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
		const FColor Color = P.bObstacleCreeping
			? FColor(255, 140, 40)
			: (P.bHasSupport ? FColor(40, 220, 120) : FColor(80, 160, 255));
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

bool USlimeSoftVolumeComponent::ContainsWorldPoint(const FVector& WorldPos) const
{
	const FVector BodyCenter = GetBodyCenterWorld();
	const float BodyR = FMath::Max(20.f, BodyRadiusCm);

	for (const FParticle& P : Particles)
	{
		if (FVector::DistSquared(WorldPos, P.WorldPos) <= FMath::Square(P.Radius))
		{
			return true;
		}
	}

	// Body envelope — soft particles are a subset; full cover uses overall blob radius too.
	return FVector::DistSquared(WorldPos, BodyCenter) <= FMath::Square(BodyR * 0.98f);
}

float USlimeSoftVolumeComponent::SampleCoverage(const TArray<FVector>& SamplePoints) const
{
	if (SamplePoints.Num() == 0)
	{
		return 0.f;
	}
	int32 Inside = 0;
	for (const FVector& Pt : SamplePoints)
	{
		if (ContainsWorldPoint(Pt))
		{
			++Inside;
		}
	}
	return static_cast<float>(Inside) / static_cast<float>(SamplePoints.Num());
}

void USlimeSoftVolumeComponent::RefreshClimbBoundsFromPrey()
{
	AActor* Prey = Climb.Prey.Get();
	if (!Prey)
	{
		Climb.bActive = false;
		return;
	}

	FVector Origin = Prey->GetActorLocation();
	FVector Extent(25.f, 25.f, 25.f);
	if (const ADevourable* Dev = Cast<ADevourable>(Prey))
	{
		if (Dev->MeshComponent)
		{
			const FBoxSphereBounds B = Dev->MeshComponent->Bounds;
			Origin = B.Origin;
			Extent = B.BoxExtent;
		}
	}
	else if (USceneComponent* Root = Prey->GetRootComponent())
	{
		const FBoxSphereBounds B = Root->Bounds;
		Origin = B.Origin;
		Extent = B.BoxExtent;
	}

	Climb.Center = Origin;
	Climb.RadiusXY = FMath::Max(18.f, FMath::Max(Extent.X, Extent.Y));
	Climb.BottomZ = Origin.Z - Extent.Z;
	Climb.TopZ = Origin.Z + Extent.Z;
	if (Climb.TopZ <= Climb.BottomZ + 1.f)
	{
		Climb.TopZ = Climb.BottomZ + 40.f;
	}
}

void USlimeSoftVolumeComponent::SetEngulfClimbTarget(AActor* Prey)
{
	// Retired (B′): press-meter + venom ribbons own engulf — do not twist soft volume.
	(void)Prey;
	ClearEngulfClimbTarget();
}

void USlimeSoftVolumeComponent::ClearEngulfClimbTarget()
{
	Climb = FEngulfClimb();
	for (FParticle& P : Particles)
	{
		P.bClimbWrapping = false;
	}
}

void USlimeSoftVolumeComponent::ApplyEngulfClimb(ASlimePawn* Pawn, float Dt, const FQuat& YawQ)
{
	// No-op — particle climb sleeve removed (laggy / unnatural). Keep flags clear.
	(void)Pawn;
	(void)Dt;
	(void)YawQ;
	for (FParticle& P : Particles)
	{
		P.bClimbWrapping = false;
	}
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
		// Keep ferrofluid sleeve supports — don't snap wrapping particles back to floor.
		if (P.bClimbWrapping)
		{
			bOutGrounded = true;
			continue;
		}

		P.bHasSupport = false;
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
			continue;
		}

		P.SupportCenter = Hit.ImpactPoint + Hit.ImpactNormal * P.Radius;
		P.bHasSupport = true;
		bOutGrounded = true;
	}
}

float USlimeSoftVolumeComponent::EstimateFloorSupportZ() const
{
	float MinSupportZ = TNumericLimits<float>::Max();
	int32 SupportCount = 0;
	for (const FParticle& P : Particles)
	{
		if (!P.bHasSupport)
		{
			continue;
		}
		MinSupportZ = FMath::Min(MinSupportZ, P.SupportCenter.Z);
		++SupportCount;
	}
	if (SupportCount == 0)
	{
		float MinPosZ = TNumericLimits<float>::Max();
		for (const FParticle& P : Particles)
		{
			MinPosZ = FMath::Min(MinPosZ, P.WorldPos.Z - P.Radius);
		}
		return MinPosZ == TNumericLimits<float>::Max() ? 0.f : MinPosZ;
	}

	// Floor cluster = supports near the lowest lip (not mid-obstacle tops).
	float Sum = 0.f;
	int32 N = 0;
	const float Band = FMath::Max(8.f, BodyRadiusCm * 0.12f);
	for (const FParticle& P : Particles)
	{
		if (!P.bHasSupport)
		{
			continue;
		}
		if (P.SupportCenter.Z <= MinSupportZ + Band)
		{
			Sum += P.SupportCenter.Z;
			++N;
		}
	}
	return N > 0 ? (Sum / N) : MinSupportZ;
}

void USlimeSoftVolumeComponent::ApplyObstacleCreepClimb(AActor* Ignore, const FVector& DesiredDelta, float Dt, bool& bOutGrounded)
{
	UWorld* World = GetWorld();
	if (!World || Particles.Num() == 0)
	{
		return;
	}

	const FVector Press(DesiredDelta.X, DesiredDelta.Y, 0.f);
	const float PressSpeed = Press.Size();
	const bool bPressing = PressSpeed > 0.5f;
	const FVector PressDir = bPressing ? (Press / PressSpeed) : FVector::ZeroVector;
	const float FloorZ = EstimateFloorSupportZ();
	const float MaxLip = BodyRadiusCm * FMath::Clamp(CreepClimbMaxHeightFrac, 0.2f, 1.4f);
	const float ClimbAlpha = 1.f - FMath::Exp(-FMath::Max(0.1f, CreepClimbRate) * Dt);
	const float PressWeight = bPressing
		? FMath::Clamp(PressSpeed / FMath::Max(1.f, BodyRadiusCm * 0.35f), 0.35f, 1.f)
		: 0.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeCreepClimb), false, Ignore);

	for (FParticle& P : Particles)
	{
		P.bObstacleCreeping = false;
		if (P.bClimbWrapping)
		{
			continue;
		}

		// Already on an elevated lip — keep partial-settle drape; stick handled in viscosity.
		if (P.bHasSupport && (P.SupportCenter.Z - FloorZ) > 5.f)
		{
			const float LipH = P.SupportCenter.Z - FloorZ;
			if (LipH <= MaxLip + P.Radius * 0.55f)
			{
				P.bObstacleCreeping = true;
				bOutGrounded = true;
			}
			continue;
		}

		if (!bPressing)
		{
			continue;
		}

		const float ProbeAhead = P.Radius * 0.85f + 6.f;
		auto TryCommitLedge = [&](const FVector& Support) -> bool
		{
			const float LipH = Support.Z - FloorZ;
			if (LipH < 3.f || LipH > MaxLip + 2.f)
			{
				return false;
			}
			const float A = ClimbAlpha * PressWeight;
			P.SupportCenter = Support;
			P.bHasSupport = true;
			P.bObstacleCreeping = true;
			P.WorldPos.Z = FMath::Lerp(P.WorldPos.Z, Support.Z, A);
			P.WorldPos.X = FMath::Lerp(P.WorldPos.X, Support.X, A * 0.5f);
			P.WorldPos.Y = FMath::Lerp(P.WorldPos.Y, Support.Y, A * 0.5f);
			if (P.Velocity.Z < 0.f)
			{
				P.Velocity.Z *= (1.f - A);
			}
			P.Velocity.Z = FMath::Max(P.Velocity.Z, 0.f);
			P.Velocity *= (1.f - A * 0.35f);
			bOutGrounded = true;
			return true;
		};

		// Overhanging a lip ahead: forward-down probe without needing a wall hit.
		{
			const FVector TopStart = P.WorldPos + PressDir * ProbeAhead + FVector(0.f, 0.f, MaxLip);
			const FVector TopEnd = P.WorldPos + PressDir * ProbeAhead - FVector(0.f, 0.f, P.Radius + SupportProbeExtraCm);
			FHitResult TopHit;
			if (World->SweepSingleByChannel(
					TopHit, TopStart, TopEnd, FQuat::Identity, ECC_WorldStatic,
					FCollisionShape::MakeSphere(P.Radius * 0.85f), Params)
				&& TopHit.ImpactNormal.Z >= GroundNormalZ)
			{
				const FVector Support = TopHit.ImpactPoint + TopHit.ImpactNormal * P.Radius;
				if (Support.Z > P.WorldPos.Z + 0.75f && TryCommitLedge(Support))
				{
					continue;
				}
			}
		}

		FHitResult WallHit;
		const FVector WallEnd = P.WorldPos + PressDir * (P.Radius + 14.f);
		if (!World->SweepSingleByChannel(
				WallHit, P.WorldPos, WallEnd, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(P.Radius * 0.9f), Params))
		{
			continue;
		}

		const FVector WallN = WallHit.bStartPenetrating ? WallHit.Normal : WallHit.ImpactNormal;
		if (WallN.Z > 0.55f || WallN.Z < -0.35f)
		{
			continue;
		}
		if (FVector::DotProduct(PressDir, WallN) > -0.05f)
		{
			continue;
		}

		// Lowest viable lip first so the climb front drapes, then follows.
		const FVector OntoTop = WallHit.ImpactPoint - WallN * (P.Radius * 0.35f) + PressDir * (P.Radius * 0.15f);
		constexpr int32 LiftSteps = 5;
		for (int32 S = 1; S <= LiftSteps; ++S)
		{
			const float Lift = (MaxLip * static_cast<float>(S)) / static_cast<float>(LiftSteps);
			const float LiftZ = FMath::Max(P.WorldPos.Z, FloorZ) + Lift + P.Radius * 0.2f;
			const FVector Lifted(OntoTop.X, OntoTop.Y, LiftZ);

			FHitResult ClearHit;
			FVector ProbeFrom = Lifted;
			const bool bBlocked = World->SweepSingleByChannel(
				ClearHit, FVector(P.WorldPos.X, P.WorldPos.Y, LiftZ), Lifted, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(P.Radius * 0.8f), Params);
			if (bBlocked)
			{
				if (ClearHit.bStartPenetrating)
				{
					continue;
				}
				ProbeFrom = ClearHit.Location;
			}

			FHitResult DownHit;
			const FVector DownEnd = ProbeFrom - FVector(0.f, 0.f, Lift + P.Radius + SupportProbeExtraCm);
			if (!World->SweepSingleByChannel(
					DownHit, ProbeFrom, DownEnd, FQuat::Identity, ECC_WorldStatic,
					FCollisionShape::MakeSphere(P.Radius * 0.85f), Params)
				|| DownHit.ImpactNormal.Z < GroundNormalZ)
			{
				continue;
			}

			const FVector Support = DownHit.ImpactPoint + DownHit.ImpactNormal * P.Radius;
			if (TryCommitLedge(Support))
			{
				break;
			}
		}
	}
}

void USlimeSoftVolumeComponent::ApplyViscosity(float Dt, const FQuat& YawQ, const FVector& BodyCenter)
{
	const float Creep = 1.f - FMath::Exp(-FMath::Max(0.1f, FormationCreepRate) * Dt);
	const float Damp = FMath::Exp(-FMath::Max(0.1f, VelocityDamping) * Dt);
	const float VertScale = FMath::Clamp(FormationVerticalScale, 0.f, 1.f);
	const float FloorZ = EstimateFloorSupportZ();
	const float StickXY = FMath::Clamp(CreepClimbStickXY, 0.f, 1.f);

	for (FParticle& P : Particles)
	{
		// Climb-wrapping particles are driven by ApplyEngulfClimb — don't yank them back to blob formation.
		if (P.bClimbWrapping)
		{
			P.Velocity *= Damp;
			continue;
		}

		const FVector Formation = BodyCenter + YawQ.RotateVector(P.RestLocal - RestComLocal);
		FVector Target = Formation;
		const bool bElevatedSupport = P.bHasSupport && ((P.SupportCenter.Z - FloorZ) > 5.f || P.bObstacleCreeping);

		if (P.bHasSupport)
		{
			// Ledge particles keep XY on the obstacle so formation doesn't yank the climb front off.
			if (bElevatedSupport)
			{
				Target.X = FMath::Lerp(Formation.X, P.SupportCenter.X, StickXY);
				Target.Y = FMath::Lerp(Formation.Y, P.SupportCenter.Y, StickXY);
			}
			else
			{
				Target.X = Formation.X;
				Target.Y = Formation.Y;
			}
			Target.Z = P.SupportCenter.Z;
		}
		else
		{
			Target.Z = FMath::Lerp(P.WorldPos.Z, Formation.Z, VertScale);
		}

		FVector ToTarget = Target - P.WorldPos;
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
			BlendTarget.Z = P.WorldPos.Z;
		}
		P.WorldPos = FMath::Lerp(P.WorldPos, BlendTarget, Creep * 0.55f);
		if (P.bHasSupport)
		{
			P.WorldPos.Z = FMath::Lerp(P.WorldPos.Z, P.SupportCenter.Z, Creep * 0.85f);
			if (bElevatedSupport)
			{
				P.WorldPos.X = FMath::Lerp(P.WorldPos.X, P.SupportCenter.X, Creep * StickXY * 0.65f);
				P.WorldPos.Y = FMath::Lerp(P.WorldPos.Y, P.SupportCenter.Y, Creep * StickXY * 0.65f);
			}
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
	// Note: climb-wrapping particles keep sleeve SupportCenter from ApplyEngulfClimb.
	const float Alpha = 1.f - FMath::Exp(-FMath::Max(0.1f, SupportSettleRate) * Dt);
	const float StickXY = FMath::Clamp(CreepClimbStickXY, 0.f, 1.f);
	for (FParticle& P : Particles)
	{
		if (!P.bHasSupport)
		{
			continue;
		}
		// Height from local support; elevated creep sticks harder in XY so the climb front holds.
		const float XYAlpha = P.bObstacleCreeping ? (Alpha * FMath::Lerp(0.25f, 0.7f, StickXY)) : (Alpha * 0.2f);
		P.WorldPos.Z = FMath::Lerp(P.WorldPos.Z, P.SupportCenter.Z, Alpha);
		P.WorldPos.X = FMath::Lerp(P.WorldPos.X, P.SupportCenter.X, XYAlpha);
		P.WorldPos.Y = FMath::Lerp(P.WorldPos.Y, P.SupportCenter.Y, XYAlpha);
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
	float MacroBoost = FMath::Clamp(SkinMacroBoost, 0.8f, 2.5f);
	float LatScale = FMath::Clamp(SkinLateralScale, 0.f, 1.f);
	const float Sigma = FMath::Max(16.f, BodyRadiusCm * FMath::Clamp(SkinSigmaScale, 0.25f, 1.f));
	const float InvTwoSig2 = 1.f / (2.f * Sigma * Sigma);

	// One-sided creep climb: slightly widen lateral skin so the silhouette reads “climbing”.
	bool bAnyObstacleCreep = false;
	for (const FParticle& P : Particles)
	{
		if (P.bObstacleCreeping)
		{
			bAnyObstacleCreep = true;
			break;
		}
	}
	if (bAnyObstacleCreep)
	{
		LatScale = FMath::Min(1.f, LatScale + 0.12f);
		MacroBoost = FMath::Min(2.5f, MacroBoost + 0.15f);
	}

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

	// 3) WASD viscous creep onto obstacle lips (particle-wise; no whole-actor StepUp).
	ApplyObstacleCreepClimb(Pawn, DesiredDelta, Dt, bOutGrounded);

	// 4) Prey climb sleeve retired (surface-drift gut owns devour) — keep call as no-op.
	ApplyEngulfClimb(Pawn, Dt, YawQ);

	// 5) Viscous formation: XY together, Z mostly free / support-driven; ledge XY stick.
	FVector Com = GetBodyCenterWorld();
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

	// 6) Creep onto local supports (viscous, no bounce).
	SettleToSupports(Dt);

	// 7) Re-resolve after viscosity (particles may have crept into geometry).
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

	// 8) Refresh supports after depen / settle; re-tag elevated lips for skin/viscosity.
	ProbeSupports(Pawn, bOutGrounded);
	{
		const float FloorZ = EstimateFloorSupportZ();
		const float MaxLip = BodyRadiusCm * FMath::Clamp(CreepClimbMaxHeightFrac, 0.2f, 1.4f);
		for (FParticle& P : Particles)
		{
			if (P.bClimbWrapping)
			{
				continue;
			}
			if (!P.bHasSupport)
			{
				P.bObstacleCreeping = false;
				continue;
			}
			const float LipH = P.SupportCenter.Z - FloorZ;
			P.bObstacleCreeping = LipH > 5.f && LipH <= MaxLip + P.Radius * 0.55f;
			if (P.bObstacleCreeping)
			{
				bOutGrounded = true;
			}
		}
	}

	// 9) Actor follows particle COM (mid-height when half-on rock — particles keep local Z).
	SyncActorFromParticles(Pawn);
	return true;
}
