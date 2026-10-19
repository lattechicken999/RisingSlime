#include "JellyDeformerComponent.h"
#include "RaisingSlimeTypes.h"
#include "Materials/MaterialInterface.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

UJellyDeformerComponent::UJellyDeformerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJellyDeformerComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildRestMesh();
	EnsureMeshComponent();
	ResetMotion();
	bReady = Rest.Num() > 0 && Mesh != nullptr;
	if (bReady)
	{
		UE_LOG(LogTemp, Log, TEXT("Jelly: ready (%d verts, %d edges)"), Rest.Num(), Edges.Num());
	}
}

void UJellyDeformerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	bReady = false;
}

void UJellyDeformerComponent::SetBodyScale(float InScale)
{
	BodyScale = FMath::Max(0.2f, InScale);
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(BodyScale));
	}
}

void UJellyDeformerComponent::SetSlimeMaterial(UMaterialInterface* Material)
{
	SlimeMaterial = Material;
	if (Mesh && Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}

void UJellyDeformerComponent::EnsureMeshComponent()
{
	if (Mesh || !GetOwner())
	{
		return;
	}

	Mesh = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("JellyMesh"));
	Mesh->SetupAttachment(GetOwner()->GetRootComponent());
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
	Mesh->RegisterComponent();
	if (SlimeMaterial)
	{
		Mesh->SetMaterial(0, SlimeMaterial);
	}
}

void UJellyDeformerComponent::BuildRestMesh()
{
	Rest.Reset();
	Triangles.Reset();
	UV0.Reset();
	Edges.Reset();

	const int32 Lat = FMath::Clamp(Latitude, 4, 64);
	const int32 Lon = FMath::Clamp(Longitude, 4, 64);
	const float Radius = 50.f; // cm
	const float Horiz = 1.05f;
	const float Vert = 0.92f;

	// North pole (Unity up Y → UE Z)
	Rest.Add(FVector(0.f, 0.f, Radius * Vert));
	UV0.Add(FVector2D(0.5f, 0.f));

	for (int32 I = 1; I < Lat; ++I)
	{
		const float V = static_cast<float>(I) / static_cast<float>(Lat);
		const float Polar = PI * V;
		const float SinP = FMath::Sin(Polar);
		const float CosP = FMath::Cos(Polar);
		for (int32 J = 0; J < Lon; ++J)
		{
			const float U = static_cast<float>(J) / static_cast<float>(Lon);
			const float Az = UE_TWO_PI * U;
			const float X = Radius * Horiz * SinP * FMath::Cos(Az);
			const float Y = Radius * Horiz * SinP * FMath::Sin(Az);
			const float Z = Radius * Vert * CosP;
			Rest.Add(FVector(X, Y, Z));
			UV0.Add(FVector2D(U, V));
		}
	}

	// South pole
	Rest.Add(FVector(0.f, 0.f, -Radius * Vert));
	UV0.Add(FVector2D(0.5f, 1.f));

	auto RingIndex = [Lon](int32 Ring, int32 J) -> int32
	{
		return 1 + Ring * Lon + (J % Lon);
	};

	// Flip winding (Unity CW → UE CCW)
	for (int32 J = 0; J < Lon; ++J)
	{
		const int32 A = 0;
		const int32 B = RingIndex(0, J);
		const int32 C = RingIndex(0, J + 1);
		Triangles.Append({A, C, B});
	}

	for (int32 Ring = 0; Ring < Lat - 2; ++Ring)
	{
		for (int32 J = 0; J < Lon; ++J)
		{
			const int32 I0 = RingIndex(Ring, J);
			const int32 I1 = RingIndex(Ring, J + 1);
			const int32 I2 = RingIndex(Ring + 1, J);
			const int32 I3 = RingIndex(Ring + 1, J + 1);
			Triangles.Append({I0, I1, I2});
			Triangles.Append({I1, I3, I2});
		}
	}

	const int32 South = Rest.Num() - 1;
	const int32 LastRing = Lat - 2;
	for (int32 J = 0; J < Lon; ++J)
	{
		const int32 B = RingIndex(LastRing, J);
		const int32 C = RingIndex(LastRing, J + 1);
		Triangles.Append({South, B, C});
	}

	TSet<uint64> Seen;
	auto AddEdge = [&](int32 A, int32 B)
	{
		if (A > B)
		{
			Swap(A, B);
		}
		const uint64 Key = (static_cast<uint64>(A) << 32) | static_cast<uint32>(B);
		if (Seen.Contains(Key))
		{
			return;
		}
		Seen.Add(Key);
		FEdge E;
		E.A = A;
		E.B = B;
		E.RestLength = FVector::Dist(Rest[A], Rest[B]);
		Edges.Add(E);
	};

	for (int32 T = 0; T + 2 < Triangles.Num(); T += 3)
	{
		AddEdge(Triangles[T], Triangles[T + 1]);
		AddEdge(Triangles[T + 1], Triangles[T + 2]);
		AddEdge(Triangles[T + 2], Triangles[T]);
	}

	const int32 N = Rest.Num();
	Pose.SetNum(N);
	Current.SetNum(N);
	Velocity.SetNum(N);
	Influence.SetNum(N);
	VertexColors.Init(FLinearColor::White, N);
	Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), N);
	RenderNormals.SetNum(N);

	checkf(N == (Lat - 1) * Lon + 2, TEXT("Expected %d verts, got %d"), (Lat - 1) * Lon + 2, N);
}

void UJellyDeformerComponent::ComputeNormals(TArray<FVector>& OutNormals) const
{
	OutNormals.SetNumZeroed(Current.Num());
	for (int32 T = 0; T + 2 < Triangles.Num(); T += 3)
	{
		const int32 I0 = Triangles[T];
		const int32 I1 = Triangles[T + 1];
		const int32 I2 = Triangles[T + 2];
		const FVector N = FVector::CrossProduct(Current[I1] - Current[I0], Current[I2] - Current[I0]).GetSafeNormal();
		OutNormals[I0] += N;
		OutNormals[I1] += N;
		OutNormals[I2] += N;
	}
	ParallelFor(OutNormals.Num(), [&](int32 Idx)
	{
		OutNormals[Idx] = OutNormals[Idx].GetSafeNormal();
		if (OutNormals[Idx].IsNearlyZero())
		{
			OutNormals[Idx] = Rest[Idx].GetSafeNormal();
		}
	});
}

void UJellyDeformerComponent::WriteMesh()
{
	if (!Mesh || Current.Num() == 0)
	{
		return;
	}

	ComputeNormals(RenderNormals);

	if (Mesh->GetNumSections() == 0)
	{
		Mesh->CreateMeshSection_LinearColor(0, Current, Triangles, RenderNormals, UV0, VertexColors, Tangents, false);
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, Current, RenderNormals, UV0, VertexColors, Tangents);
	}
	if (SlimeMaterial)
	{
		Mesh->SetMaterial(0, SlimeMaterial);
	}
}

float UJellyDeformerComponent::SmoothStep(float T)
{
	T = FMath::Clamp(T, 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

void UJellyDeformerComponent::ClampPose()
{
	const float MaxD = MaxDeformCm();
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		FVector Off = Pose[I] - Rest[I];
		FVector Horiz(Off.X, Off.Y, 0.f);
		if (Horiz.Size() > MaxD)
		{
			Horiz = Horiz.GetSafeNormal() * MaxD;
		}
		Off.Z = FMath::Clamp(Off.Z, -MaxD, MaxD);
		Pose[I] = Rest[I] + FVector(Horiz.X, Horiz.Y, Off.Z);
	}
}

void UJellyDeformerComponent::ClampCurrentToRest()
{
	const float MaxD = MaxDeformCm();
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		FVector Off = Current[I] - Rest[I];
		FVector Horiz(Off.X, Off.Y, 0.f);
		if (Horiz.Size() > MaxD)
		{
			Horiz = Horiz.GetSafeNormal() * MaxD;
		}
		Off.Z = FMath::Clamp(Off.Z, -MaxD, MaxD);
		Current[I] = Rest[I] + FVector(Horiz.X, Horiz.Y, Off.Z);
	}
}

void UJellyDeformerComponent::BuildPose(const FVector& LocalVelocityCms)
{
	const FVector V = LocalVelocityCms * 0.01f; // cm/s → m/s for formulas
	const float Elongate = FMath::Min(FMath::Abs(V.Z) * Settings.StretchStrength * Settings.SquashStrength * 0.15f, 2.4f);
	const float Pinch = Elongate * 0.45f;
	const float PlanarSpeed = FVector2D(V.X, V.Y).Size();
	const float Trail = FMath::Min(PlanarSpeed * Settings.MovementDeformation * 0.12f, Settings.MaximumDeformationM);
	const FVector PlanarDir = FVector(V.X, V.Y, 0.f).GetSafeNormal();
	const float MaxD = MaxDeformCm();

	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const FVector R = Rest[I] * 0.01f; // cm → m for offset math, then *100
		FVector OffsetM(-R.X * Pinch, -R.Y * Pinch, R.Z * Elongate);
		if (!PlanarDir.IsNearlyZero())
		{
			const float Face = FVector::DotProduct(R.GetSafeNormal(), PlanarDir);
			OffsetM += PlanarDir * (Trail * Face);
		}
		if (OffsetM.Size() > Settings.MaximumDeformationM)
		{
			OffsetM = OffsetM.GetSafeNormal() * Settings.MaximumDeformationM;
		}
		Pose[I] = Rest[I] + OffsetM * 100.f;
		Influence[I] = 0.f;
	}
	ClampPose();
}

void UJellyDeformerComponent::EnqueueContact(const FVector& LocalPoint, const FVector& LocalNormal, float PressureCm, float RadiusCm)
{
	if (!bReady || PressureCm <= KINDA_SMALL_NUMBER || RadiusCm <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (PendingContacts.Num() >= MaxContactsPerStep)
	{
		return;
	}
	FPendingContact C;
	C.LocalPoint = LocalPoint;
	C.LocalNormal = LocalNormal.GetSafeNormal();
	C.PressureCm = FMath::Min(PressureCm, Settings.MaxContactDeformationM * 100.f);
	C.RadiusCm = RadiusCm;
	if (!C.LocalNormal.IsNearlyZero())
	{
		PendingContacts.Add(C);
	}
}

void UJellyDeformerComponent::GatherWorldContacts(const FVector& WorldVelocityCms)
{
	PendingContacts.Reset();
	if (!Settings.bContactDeformation || !Settings.bWorldContactProbe || !bReady || !Mesh || !GetWorld() || !GetOwner())
	{
		return;
	}

	const float WorldRadiusCm = RestRadiusCm() * BodyScale;
	const FVector Center = Mesh->GetComponentLocation();
	const FTransform MeshXf = Mesh->GetComponentTransform();
	const float ProbeReach = WorldRadiusCm * 1.2f;
	const float ProbeSphere = FMath::Clamp(WorldRadiusCm * 0.2f, 6.f, 18.f);
	const float MaxPressCm = Settings.MaxContactDeformationM * 100.f;
	const float ContactRadCm = Settings.ContactRadiusM * 100.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JellyWorldContact), false, GetOwner());
	Params.bReturnPhysicalMaterial = false;
	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjQuery.AddObjectTypesToQuery(ECC_Vehicle);

	TArray<FVector> Dirs;
	Dirs.Reserve(14);
	// Floor / ceiling extras (standing squash is also ApplyGround; this catches pads/ledges).
	Dirs.Add(FVector(0.f, 0.f, -1.f));
	Dirs.Add(FVector(0.f, 0.f, 1.f));
	// Horizontal ring — walls.
	for (int32 I = 0; I < 8; ++I)
	{
		const float Ang = static_cast<float>(I) * (UE_TWO_PI / 8.f);
		Dirs.Add(FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f));
	}
	// Mild downward diagonals for curb / rock contact.
	for (int32 I = 0; I < 4; ++I)
	{
		const float Ang = static_cast<float>(I) * (UE_TWO_PI / 4.f) + 0.4f;
		Dirs.Add(FVector(FMath::Cos(Ang), FMath::Sin(Ang), -0.55f).GetSafeNormal());
	}
	// Prefer travel direction so walking into a wall reads immediately.
	FVector Travel = FVector(WorldVelocityCms.X, WorldVelocityCms.Y, 0.f);
	if (Travel.SizeSquared() > 400.f) // >20 cm/s
	{
		Dirs.Insert(Travel.GetSafeNormal(), 0);
	}

	for (const FVector& WorldDir : Dirs)
	{
		if (PendingContacts.Num() >= MaxContactsPerStep)
		{
			break;
		}
		const FVector Dir = WorldDir.GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			continue;
		}

		FHitResult Hit;
		const FVector Start = Center;
		const FVector End = Center + Dir * ProbeReach;
		if (!GetWorld()->SweepSingleByObjectType(
			Hit, Start, End, FQuat::Identity, ObjQuery, FCollisionShape::MakeSphere(ProbeSphere), Params))
		{
			continue;
		}
		if (!Hit.bBlockingHit && !Hit.bStartPenetrating)
		{
			continue;
		}
		if (Hit.GetActor() == GetOwner())
		{
			continue;
		}

		const float Dist = FVector::Distance(Center, Hit.ImpactPoint);
		float PenetrationCm = (WorldRadiusCm * 0.94f) - Dist;
		if (Hit.bStartPenetrating)
		{
			PenetrationCm = FMath::Max(PenetrationCm, Hit.PenetrationDepth);
		}
		if (PenetrationCm < 1.5f)
		{
			continue;
		}

		const float PressCm = FMath::Clamp(PenetrationCm * Settings.ContactStrength * Settings.CollisionDeformation, 0.f, MaxPressCm);
		const FVector LocalPoint = MeshXf.InverseTransformPosition(Hit.ImpactPoint);
		const FVector LocalNormal = MeshXf.InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
		EnqueueContact(LocalPoint, LocalNormal, PressCm, ContactRadCm);
	}
}

void UJellyDeformerComponent::ApplyQueuedContacts()
{
	if (!Settings.bContactDeformation || bAscending || ActiveContacts.Num() == 0)
	{
		return;
	}

	const float InvSqrt = 1.f / FMath::Sqrt(static_cast<float>(ActiveContacts.Num()));
	for (const FPendingContact& C : ActiveContacts)
	{
		const FVector Dent = C.LocalNormal.GetSafeNormal();
		if (Dent.IsNearlyZero())
		{
			continue;
		}
		const float PressCm = C.PressureCm * InvSqrt;
		for (int32 I = 0; I < Rest.Num(); ++I)
		{
			const float Facing = FVector::DotProduct(Rest[I].GetSafeNormal(), -Dent);
			if (Facing < 0.12f)
			{
				continue;
			}
			const float Dist = FVector::Dist(Rest[I], C.LocalPoint);
			if (Dist > C.RadiusCm)
			{
				continue;
			}
			const float T = 1.f - Dist / C.RadiusCm;
			float Inf = SmoothStep(T) * FMath::Clamp((Facing - 0.12f) / 0.7f, 0.f, 1.f);
			if (Settings.ContactFalloff > 0.01f && Settings.ContactFalloff < 0.99f)
			{
				Inf = FMath::Pow(Inf, Settings.ContactFalloff);
			}
			else if (Settings.ContactFalloff > 1.01f)
			{
				Inf = FMath::Pow(Inf, Settings.ContactFalloff);
			}
			if (Inf < 0.001f)
			{
				continue;
			}
			Influence[I] = FMath::Max(Influence[I], Inf);
			// Push verts along impact normal (into body) so surface flattens against the object.
			Pose[I] += Dent * (PressCm * Inf);
		}
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplyGround(float Dt, bool bGrounded, float GroundImpactSpeedMps)
{
	const float MaxLand = Settings.MaxLandingDeformationM;
	if (!Settings.bGroundContact || !bGrounded)
	{
		const float Pull = 1.f - FMath::Exp(-6.f * Dt);
		LandingPress = FMath::Lerp(LandingPress, 0.f, Pull);
		LandingVelocity = FMath::Lerp(LandingVelocity, 0.f, Pull);
		bWasGrounded = false;
		return;
	}

	if (Settings.bLandingImpact && !bWasGrounded && GroundImpactSpeedMps >= Settings.LandingVelocityThresholdMps)
	{
		const float Peak = FMath::Min(GroundImpactSpeedMps * Settings.LandingImpactStrength, MaxLand);
		const float Omega = FMath::Max(1.f, Settings.LandingRecoverySpeed);
		LandingVelocity += Peak * Omega * Settings.LandingBounceStrength;
	}
	bWasGrounded = true;

	{
		const float Omega = FMath::Max(1.f, Settings.LandingRecoverySpeed);
		const float Damping = 2.f * 0.48f * Omega;
		const float Accel = -Omega * Omega * LandingPress - Damping * LandingVelocity;
		LandingVelocity += Accel * Dt;
		LandingPress += LandingVelocity * Dt;
		LandingPress = FMath::Clamp(LandingPress, -MaxLand * 0.35f, MaxLand);
	}

	// Steady floor sit squash (always-on while grounded) + landing pulse.
	float CompressionM = Settings.GroundContactStrengthM + LandingPress;
	const float Cap = Settings.MaxGroundCompressionM + MaxLand;
	CompressionM = FMath::Clamp(CompressionM, -MaxLand * 0.35f, Cap);

	const FVector Normal(0.f, 0.f, 1.f);
	const float BandM = FMath::Max(Settings.GroundDeformationHeightM, 0.2f);
	const float GroundZ = -RestRadiusCm() * 0.92f;

	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const float HeightM = (Rest[I].Z - GroundZ) * 0.01f;
		if (HeightM < 0.f || HeightM > BandM)
		{
			continue;
		}
		const float T = 1.f - HeightM / BandM;
		float Inf = SmoothStep(T);
		if (Settings.GroundContactFalloff > 0.01f && !FMath::IsNearlyEqual(Settings.GroundContactFalloff, 1.f))
		{
			Inf = FMath::Pow(Inf, Settings.GroundContactFalloff);
		}
		if (Inf < 0.001f)
		{
			continue;
		}
		Influence[I] = FMath::Max(Influence[I], Inf);
		// Lift bottom verts → flatten against floor.
		Pose[I] += Normal * (CompressionM * Inf * 100.f);
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplyVolumePreservation()
{
	if (!Settings.bVolumePreservation || Settings.VolumePreservation <= KINDA_SMALL_NUMBER || Rest.Num() == 0)
	{
		return;
	}

	// Mean |Z| and mean planar radius as a cheap volume proxy (cm).
	double SumAbsZRest = 0.0;
	double SumAbsZPose = 0.0;
	double SumRadRest = 0.0;
	double SumRadPose = 0.0;
	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		SumAbsZRest += FMath::Abs(static_cast<double>(Rest[I].Z));
		SumAbsZPose += FMath::Abs(static_cast<double>(Pose[I].Z));
		SumRadRest += FVector2D(Rest[I].X, Rest[I].Y).Size();
		SumRadPose += FVector2D(Pose[I].X, Pose[I].Y).Size();
	}
	const double InvN = 1.0 / static_cast<double>(Rest.Num());
	const float MeanZRest = static_cast<float>(SumAbsZRest * InvN);
	const float MeanZPose = static_cast<float>(FMath::Max(SumAbsZPose * InvN, 1.0));
	const float MeanRRest = static_cast<float>(FMath::Max(SumRadRest * InvN, 1.0));
	const float MeanRPose = static_cast<float>(FMath::Max(SumRadPose * InvN, 1.0));

	const float VertScale = MeanZPose / FMath::Max(MeanZRest, 1.f);
	const float VertClamped = FMath::Clamp(VertScale, 1.f - Settings.MaxVerticalCompressionM / 0.5f, Settings.MaximumStretch);
	const float TargetRadial = MeanRRest * FMath::Sqrt(FMath::Max(1.f / FMath::Max(VertClamped, 0.35f), 0.35f));
	float RadialScale = TargetRadial / MeanRPose;
	RadialScale = FMath::Lerp(1.f, RadialScale, Settings.VolumePreservation);

	const float MaxExpandCm = Settings.MaxRadialExpansionM * 100.f;
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		FVector P = Pose[I];
		FVector2D Horiz(P.X, P.Y);
		const float R0 = Horiz.Size();
		if (R0 > KINDA_SMALL_NUMBER)
		{
			float NewR = R0 * RadialScale;
			if (Settings.bRadialExpansion)
			{
				const float RestR = FVector2D(Rest[I].X, Rest[I].Y).Size();
				NewR = FMath::Min(NewR, RestR + MaxExpandCm);
			}
			Horiz *= (NewR / R0);
			P.X = Horiz.X;
			P.Y = Horiz.Y;
		}

		// Soft vertical clamp toward rest for extreme squash.
		const float MaxVertCm = Settings.MaxVerticalCompressionM * 100.f;
		const float ZOff = P.Z - Rest[I].Z;
		P.Z = Rest[I].Z + FMath::Clamp(ZOff, -MaxVertCm, MaxDeformCm());
		Pose[I] = P;
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplyInertia(const FVector& LocalAccelCms)
{
	const FVector DeltaMps = LocalAccelCms * 0.01f; // treat accel*dt already passed as delta-v approx
	const FVector Kick = -DeltaMps * Settings.MovementDeformation * 100.f; // to cm/s kick
	if (Kick.IsNearlyZero())
	{
		return;
	}
	const FVector KickDir = Kick.GetSafeNormal();
	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const float Face = FVector::DotProduct(Rest[I].GetSafeNormal(), KickDir);
		const float W = FMath::Lerp(0.35f, 1.f, FMath::Clamp((Face + 0.5f) / 1.f, 0.f, 1.f));
		Velocity[I] += Kick * (W * Settings.StretchStrength);
	}
}

void UJellyDeformerComponent::StepSprings(float Dt)
{
	ClampCurrentToRest();
	const float MaxD = MaxDeformCm();
	const float MaxSpeed = MaxD * 12.f;

	TArray<FVector> Force;
	Force.SetNumZeroed(Current.Num());

	// Soft follow of pose — low K + higher damp = 물렁 squash that settles slowly.
	const float K = Settings.SpringStrength * Settings.RecoverySpeed;
	const float Damp = Settings.Damping * (0.9f + 0.2f * (1.f - Settings.RecoverySpeed));
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		Force[I] += -K * (Current[I] - Pose[I]) - Damp * Velocity[I];
	}

	for (const FEdge& E : Edges)
	{
		const FVector CurDelta = Current[E.B] - Current[E.A];
		const FVector PoseDelta = Pose[E.B] - Pose[E.A];
		const FVector Term = (CurDelta - PoseDelta) * Settings.NeighborStrength;
		Force[E.A] += Term;
		Force[E.B] -= Term;
	}

	for (int32 I = 0; I < Current.Num(); ++I)
	{
		Velocity[I] += Force[I] * Dt;
		if (Velocity[I].Size() > MaxSpeed)
		{
			Velocity[I] = Velocity[I].GetSafeNormal() * MaxSpeed;
		}
		Current[I] += Velocity[I] * Dt;
	}

	for (const FEdge& E : Edges)
	{
		FVector Delta = Current[E.B] - Current[E.A];
		const float Len = Delta.Size();
		if (Len < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float PoseLen = FVector::Dist(Pose[E.A], Pose[E.B]);
		const float LimitHigh = FMath::Max(PoseLen, E.RestLength) * Settings.MaximumStretch;
		const float LimitLow = E.RestLength / Settings.MaximumStretch;
		if (Len > LimitHigh || Len < LimitLow)
		{
			const float Target = FMath::Clamp(Len, LimitLow, LimitHigh);
			const FVector Corr = Delta.GetSafeNormal() * ((Len - Target) * 0.5f);
			Current[E.A] += Corr;
			Current[E.B] -= Corr;
		}
	}
}

void UJellyDeformerComponent::Step(float Dt, const FVector& WorldVelocityCms, const FVector& WorldAccelCms, bool bGrounded, float GroundImpactSpeedMps)
{
	if (!bReady)
	{
		return;
	}

	// World → mesh local (actor yaw only; mesh up is Z)
	AActor* Owner = GetOwner();
	const FRotator YawRot(0.f, Owner ? Owner->GetActorRotation().Yaw : 0.f, 0.f);
	const FVector LocalVel = YawRot.UnrotateVector(WorldVelocityCms);
	const FVector LocalAcc = YawRot.UnrotateVector(WorldAccelCms);
	bAscending = LocalVel.Z > 150.f;

	GatherWorldContacts(WorldVelocityCms);
	ActiveContacts = MoveTemp(PendingContacts);
	PendingContacts.Reset();

	BuildPose(LocalVel);
	ApplyQueuedContacts();
	ApplyGround(Dt, bGrounded, GroundImpactSpeedMps);
	ApplyVolumePreservation();
	ApplyInertia(LocalAcc);
	StepSprings(Dt);
	WriteMesh();

	PrevWorldVelocityCms = WorldVelocityCms;
}

void UJellyDeformerComponent::ResetMotion()
{
	LandingPress = 0.f;
	LandingVelocity = 0.f;
	bWasGrounded = false;
	PendingContacts.Reset();
	ActiveContacts.Reset();
	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		Pose[I] = Rest[I];
		Current[I] = Rest[I];
		Velocity[I] = FVector::ZeroVector;
		Influence[I] = 0.f;
	}
	if (Mesh && Current.Num() > 0)
	{
		WriteMesh();
	}
}

void UJellyDeformerComponent::AddImpulse(const FVector& LocalPoint, const FVector& LocalDirection, float Magnitude, float Radius, float Squash, float Stretch)
{
	if (!bReady || Radius < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Dir = LocalDirection.GetSafeNormal();
	FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::CrossProduct(Dir, FVector::RightVector).GetSafeNormal();
	}

	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const float Facing = FVector::DotProduct(Rest[I].GetSafeNormal(), -Dir);
		if (Facing < 0.2f)
		{
			continue;
		}
		const float Dist = FVector::Dist(Rest[I], LocalPoint);
		if (Dist > Radius)
		{
			continue;
		}
		const float Falloff = 1.f - SmoothStep(Dist / Radius);
		Velocity[I] += Dir * (Magnitude * Squash * Falloff);
		if (!Side.IsNearlyZero())
		{
			Velocity[I] += Side * (Magnitude * Stretch * Falloff * 0.28f);
		}
	}
}
