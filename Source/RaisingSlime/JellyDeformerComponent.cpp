#include "JellyDeformerComponent.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
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

	// North pole (Unity up Y ??UE Z)
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

	// Flip winding (Unity CW ??UE CCW)
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
	const float MaxZ = MaxVerticalStretchCm();
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		FVector Off = Pose[I] - Rest[I];
		FVector Horiz(Off.X, Off.Y, 0.f);
		if (Horiz.Size() > MaxD)
		{
			Horiz = Horiz.GetSafeNormal() * MaxD;
		}
		Off.Z = FMath::Clamp(Off.Z, -MaxZ, MaxZ);
		Pose[I] = Rest[I] + FVector(Horiz.X, Horiz.Y, Off.Z);
	}
}

void UJellyDeformerComponent::ClampCurrentToRest()
{
	const float MaxD = MaxDeformCm();
	const float MaxZ = MaxVerticalStretchCm();
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		FVector Off = Current[I] - Rest[I];
		FVector Horiz(Off.X, Off.Y, 0.f);
		if (Horiz.Size() > MaxD)
		{
			Horiz = Horiz.GetSafeNormal() * MaxD;
		}
		Off.Z = FMath::Clamp(Off.Z, -MaxZ, MaxZ);
		Current[I] = Rest[I] + FVector(Horiz.X, Horiz.Y, Off.Z);
	}
}

void UJellyDeformerComponent::BuildPose(const FVector& LocalVelocityCms, const FVector& LocalAccelCms)
{
	const FVector V = LocalVelocityCms * 0.01f; // cm/s → m/s
	const FVector A = LocalAccelCms * 0.01f;
	const float Ref = FMath::Max(1.f, Settings.JumpDeformRefSpeedMps);
	const float JumpT = SmoothStep(FMath::Clamp(FMath::Abs(V.Z) / Ref, 0.f, 1.f));

	// Viscous liquid jump: mild height change + volume-conserving radius (never pinch through zero).
	float VertMul = 1.f;
	if (V.Z > 0.35f)
	{
		VertMul = 1.f + JumpT * Settings.JumpRiseStretch;
		// Takeoff impulse: brief flatten before compact rise (not taffy launch).
		if (A.Z > 8.f)
		{
			const float FlatT = SmoothStep(FMath::Clamp((A.Z - 8.f) / 25.f, 0.f, 1.f));
			VertMul = FMath::Lerp(VertMul, 1.f - Settings.JumpTakeoffFlatten, FlatT);
		}
	}
	else if (V.Z < -0.35f)
	{
		VertMul = 1.f - JumpT * Settings.JumpFallSquash;
	}
	VertMul = FMath::Clamp(VertMul, 1.f / Settings.MaximumStretch, Settings.MaximumStretch);
	const float RadMul = FMath::Sqrt(1.f / FMath::Max(VertMul, 0.35f));

	const float PlanarSpeed = FVector2D(V.X, V.Y).Size();
	const float Trail = FMath::Min(PlanarSpeed * Settings.MovementDeformation * 0.12f, Settings.MaximumDeformationM * 0.45f);
	const FVector PlanarDir = FVector(V.X, V.Y, 0.f).GetSafeNormal();

	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		FVector P(Rest[I].X * RadMul, Rest[I].Y * RadMul, Rest[I].Z * VertMul);
		if (!PlanarDir.IsNearlyZero() && Trail > KINDA_SMALL_NUMBER)
		{
			const float Face = FVector::DotProduct(Rest[I].GetSafeNormal(), PlanarDir);
			P += PlanarDir * (Trail * Face * 100.f);
		}
		Pose[I] = P;
		Influence[I] = 0.f;
	}
	ClampPose();
}

void UJellyDeformerComponent::EnqueueContact(const FVector& LocalPoint, const FVector& LocalNormal, float PressureCm, float RadiusCm, float IntoSpeedCms)
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
	C.TargetPressureCm = FMath::Min(PressureCm, Settings.MaxContactDeformationM * 100.f);
	C.RadiusCm = RadiusCm;
	C.IntoSpeedCms = FMath::Max(0.f, IntoSpeedCms);
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
	const float ProbeReach = WorldRadiusCm * 1.15f;
	const float ProbeSphere = FMath::Clamp(WorldRadiusCm * 0.18f, 5.f, 16.f);
	const float MaxPressCm = Settings.MaxContactDeformationM * 100.f;
	const float ContactRadCm = Settings.ContactRadiusM * 100.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JellyWorldContact), false, GetOwner());
	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjQuery.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FVector> Dirs;
	Dirs.Add(FVector(0.f, 0.f, -1.f));
	for (int32 I = 0; I < 8; ++I)
	{
		const float Ang = static_cast<float>(I) * (UE_TWO_PI / 8.f);
		Dirs.Add(FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f));
	}
	for (int32 I = 0; I < 4; ++I)
	{
		const float Ang = static_cast<float>(I) * (UE_TWO_PI / 4.f) + 0.35f;
		Dirs.Add(FVector(FMath::Cos(Ang), FMath::Sin(Ang), -0.5f).GetSafeNormal());
	}
	FVector Travel(WorldVelocityCms.X, WorldVelocityCms.Y, 0.f);
	if (Travel.SizeSquared() > 400.f)
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
		if (!GetWorld()->SweepSingleByObjectType(
			Hit, Center, Center + Dir * ProbeReach, FQuat::Identity, ObjQuery,
			FCollisionShape::MakeSphere(ProbeSphere), Params))
		{
			continue;
		}
		if ((!Hit.bBlockingHit && !Hit.bStartPenetrating) || Hit.GetActor() == GetOwner())
		{
			continue;
		}

		const float Dist = FVector::Distance(Center, Hit.ImpactPoint);
		float PenetrationCm = (WorldRadiusCm * 0.92f) - Dist;
		if (Hit.bStartPenetrating)
		{
			PenetrationCm = FMath::Max(PenetrationCm, Hit.PenetrationDepth);
		}
		if (PenetrationCm < 1.f)
		{
			continue;
		}

		const FVector N = Hit.ImpactNormal.GetSafeNormal();
		const float Into = FMath::Max(0.f, FVector::DotProduct(-WorldVelocityCms, N));
		const float PressCm = FMath::Clamp(
			(PenetrationCm * Settings.ContactStrength + Into * Settings.IntoSpeedScale) * Settings.CollisionDeformation,
			0.f, MaxPressCm);

		EnqueueContact(
			MeshXf.InverseTransformPosition(Hit.ImpactPoint),
			MeshXf.InverseTransformVectorNoScale(N).GetSafeNormal(),
			PressCm, ContactRadCm, Into);
	}
}

void UJellyDeformerComponent::GatherEngulfObstacles()
{
	PendingEngulfs.Reset();
	if (!Settings.bUndersideEngulf || !Settings.bUndersideEngulfFeature || !bReady || !Mesh || !GetWorld() || !GetOwner())
	{
		return;
	}

	const float WorldRadiusCm = RestRadiusCm() * BodyScale;
	const float WorldDiameterCm = WorldRadiusCm * 2.f;
	// Prey_Rock ≈55–70cm tall; must compare to diameter (~100cm), not radius (~50cm).
	const float MaxEngulfH = WorldDiameterCm * Settings.EngulfMaxHeightFractionOfDiameter;
	const FVector Center = Mesh->GetComponentLocation();
	const float FeetZ = Center.Z - WorldRadiusCm;
	const FTransform MeshXf = Mesh->GetComponentTransform();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JellyEngulf), false, GetOwner());
	Params.bTraceComplex = false;

	const float ProbeSphere = FMath::Clamp(WorldRadiusCm * 0.14f, 5.f, 14.f);

	TArray<FVector> Offsets;
	Offsets.Add(FVector::ZeroVector);
	for (int32 Ring = 1; Ring <= 2; ++Ring)
	{
		const float R = WorldRadiusCm * (0.32f * Ring);
		const int32 Segs = Ring == 1 ? 6 : 8;
		for (int32 I = 0; I < Segs; ++I)
		{
			const float Ang = static_cast<float>(I) * (UE_TWO_PI / static_cast<float>(Segs));
			Offsets.Add(FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0.f));
		}
	}

	auto TryAcceptHit = [&](const FHitResult& Hit) -> bool
	{
		if ((!Hit.bBlockingHit && !Hit.bStartPenetrating) || !Hit.GetActor() || Hit.GetActor() == GetOwner())
		{
			return false;
		}
		// Skip near-vertical walls — face squash handles those.
		if (Hit.ImpactNormal.Z < 0.25f)
		{
			return false;
		}

		const float HeightAboveFeet = Hit.ImpactPoint.Z - FeetZ;
		// Flat floor ≈ 0; reject. Tall walls / buildings exceed MaxEngulfH.
		if (HeightAboveFeet < Settings.EngulfMinHeightCm || HeightAboveFeet > MaxEngulfH)
		{
			return false;
		}

		const FVector LocalTop = MeshXf.InverseTransformPosition(Hit.ImpactPoint);
		// Allow mid-body intrusion (Hunt rocks poke above mesh equator). Only skip ceiling-ish.
		if (LocalTop.Z > RestRadiusCm() * 0.85f)
		{
			return false;
		}

		int32 Best = INDEX_NONE;
		float BestDist = 22.f;
		for (int32 I = 0; I < PendingEngulfs.Num(); ++I)
		{
			const float D = FVector::Dist2D(PendingEngulfs[I].LocalTop, LocalTop);
			if (D < BestDist)
			{
				BestDist = D;
				Best = I;
			}
		}

		const float InflR = (WorldRadiusCm * Settings.EngulfInfluenceRadiusScale) / FMath::Max(BodyScale, 0.2f);
		const float Strength = FMath::Clamp(SmoothStep(1.f - HeightAboveFeet / MaxEngulfH) + 0.35f, 0.35f, 1.f);
		const float HeightLocal = HeightAboveFeet / FMath::Max(BodyScale, 0.2f);

		if (Best == INDEX_NONE)
		{
			if (PendingEngulfs.Num() >= MaxEngulfsPerStep)
			{
				return false;
			}
			FEngulfSite E;
			E.LocalTop = LocalTop;
			E.HeightCm = HeightLocal;
			E.RadiusCm = InflR;
			E.TargetStrength = Strength;
			E.Strength = Strength;
			PendingEngulfs.Add(E);
		}
		else
		{
			FEngulfSite& E = PendingEngulfs[Best];
			E.LocalTop = FMath::Lerp(E.LocalTop, LocalTop, 0.5f);
			E.HeightCm = FMath::Max(E.HeightCm, HeightLocal);
			E.RadiusCm = FMath::Max(E.RadiusCm, InflR);
			E.TargetStrength = FMath::Max(E.TargetStrength, Strength);
			E.Strength = FMath::Max(E.Strength, Strength * 0.85f);
		}

		if (Settings.bEngulfDebugLog)
		{
			static float LastLogTime = -100.f;
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now - LastLogTime > 0.4f)
			{
				LastLogTime = Now;
				UE_LOG(LogTemp, Display,
					TEXT("JellyEngulf: hit=%s hAboveFeet=%.1fcm (max=%.1f) localZ=%.1f wrapH=%.1f"),
					*Hit.GetActor()->GetName(), HeightAboveFeet, MaxEngulfH, LocalTop.Z, HeightLocal);
			}
		}
		return true;
	};

	for (const FVector& Off : Offsets)
	{
		if (PendingEngulfs.Num() >= MaxEngulfsPerStep)
		{
			break;
		}

		// Trace through the full body column — rock tops often sit ABOVE mesh center on Hunt.
		const FVector Start = Center + Off + FVector(0.f, 0.f, WorldRadiusCm * 0.35f);
		const FVector End = Center + Off - FVector(0.f, 0.f, WorldRadiusCm * 1.15f);

		FHitResult HitDyn;
		FHitResult HitStat;
		const bool bDyn = GetWorld()->SweepSingleByChannel(
			HitDyn, Start, End, FQuat::Identity, ECC_WorldDynamic,
			FCollisionShape::MakeSphere(ProbeSphere), Params);
		const bool bStat = GetWorld()->SweepSingleByChannel(
			HitStat, Start, End, FQuat::Identity, ECC_WorldStatic,
			FCollisionShape::MakeSphere(ProbeSphere), Params);

		if (bDyn)
		{
			TryAcceptHit(HitDyn);
		}
		if (bStat)
		{
			TryAcceptHit(HitStat);
		}
	}
}

void UJellyDeformerComponent::ApplySoftVolumeSkin()
{
	const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner());
	const USlimeSoftVolumeComponent* Soft = Pawn ? Pawn->GetSoftVolume() : nullptr;
	if (!Soft || !Soft->bEnabled || Soft->GetParticleCount() == 0 || Pose.Num() == 0)
	{
		return;
	}

	const float InvScale = 1.f / FMath::Max(BodyScale, 0.2f);
	// Airborne: damp soft-volume Z so particle lag can't stretch into a taffy strand.
	// Grounded / partial settle keeps full macro drape.
	const float AirZScale = (Soft->GetSupportedParticleCount() >= 2) ? 1.f : 0.2f;
	TArray<FVector> SoftDisp;
	SoftDisp.SetNumUninitialized(Pose.Num());
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		FVector D = Soft->SampleDisplacementLocal(Rest[I] * BodyScale) * InvScale;
		D.Z *= AirZScale;
		SoftDisp[I] = D;
	}

	// Laplacian on XY only — Z is the macro drape and must not be flattened.
	if (Edges.Num() > 0)
	{
		TArray<FVector> SmoothDisp;
		SmoothDisp.SetNumUninitialized(SoftDisp.Num());
		TArray<float> WSum;
		WSum.Init(1.f, SoftDisp.Num());
		for (int32 I = 0; I < SoftDisp.Num(); ++I)
		{
			SmoothDisp[I] = SoftDisp[I];
		}
		for (const FEdge& E : Edges)
		{
			SmoothDisp[E.A] += SoftDisp[E.B];
			SmoothDisp[E.B] += SoftDisp[E.A];
			WSum[E.A] += 1.f;
			WSum[E.B] += 1.f;
		}
		constexpr float LapBlendXY = 0.5f;
		for (int32 I = 0; I < SoftDisp.Num(); ++I)
		{
			const FVector Avg = SmoothDisp[I] / WSum[I];
			SoftDisp[I].X = FMath::Lerp(SoftDisp[I].X, Avg.X, LapBlendXY);
			SoftDisp[I].Y = FMath::Lerp(SoftDisp[I].Y, Avg.Y, LapBlendXY);
			// SoftDisp[I].Z unchanged — preserves climb / partial-settle silhouette.
		}
	}

	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		Pose[I] += SoftDisp[I];
		if (Current.IsValidIndex(I))
		{
			// Z hard-follows macro drape; XY stays viscous.
			Current[I].Z = Pose[I].Z;
			Current[I].X = FMath::Lerp(Current[I].X, Pose[I].X, 0.4f);
			Current[I].Y = FMath::Lerp(Current[I].Y, Pose[I].Y, 0.4f);
		}
	}
}

void UJellyDeformerComponent::UpdateSmoothedEngulfs(float Dt, float StepUpCm)
{
	for (const FEngulfSite& P : PendingEngulfs)
	{
		int32 Best = INDEX_NONE;
		float BestDist = 28.f;
		for (int32 I = 0; I < SmoothedEngulfs.Num(); ++I)
		{
			const float D = FVector::Dist2D(SmoothedEngulfs[I].LocalTop, P.LocalTop);
			if (D < BestDist)
			{
				BestDist = D;
				Best = I;
			}
		}
		const float Boost = 1.f + FMath::Clamp(StepUpCm / 28.f, 0.f, 1.f) * Settings.EngulfStepUpBoost;
		if (Best == INDEX_NONE)
		{
			if (SmoothedEngulfs.Num() >= MaxEngulfsPerStep)
			{
				continue;
			}
			FEngulfSite S = P;
			S.TargetStrength = FMath::Clamp(P.TargetStrength * Boost, 0.f, 1.f);
			S.Strength = S.TargetStrength; // snap on — no long fade-in that hides the effect
			S.Age = 0.f;
			SmoothedEngulfs.Add(S);
		}
		else
		{
			FEngulfSite& S = SmoothedEngulfs[Best];
			S.LocalTop = FMath::Lerp(S.LocalTop, P.LocalTop, 0.55f);
			S.HeightCm = FMath::Max(S.HeightCm, P.HeightCm);
			S.RadiusCm = FMath::Lerp(S.RadiusCm, P.RadiusCm, 0.45f);
			S.TargetStrength = FMath::Clamp(P.TargetStrength * Boost, 0.f, 1.f);
			S.Strength = FMath::Max(S.Strength, S.TargetStrength * 0.9f);
			S.Age = 0.f;
		}
	}

	const float Alpha = 1.f - FMath::Exp(-FMath::Max(1.f, Settings.EngulfSmoothRate) * Dt);
	for (int32 I = SmoothedEngulfs.Num() - 1; I >= 0; --I)
	{
		FEngulfSite& S = SmoothedEngulfs[I];
		S.Age += Dt;
		if (S.Age > Dt * 1.5f)
		{
			S.TargetStrength = FMath::Lerp(S.TargetStrength, 0.f, Alpha);
		}
		S.Strength = FMath::Lerp(S.Strength, S.TargetStrength, Alpha);
		if (S.Strength < 0.03f && S.TargetStrength < 0.03f && S.Age > 0.12f)
		{
			SmoothedEngulfs.RemoveAtSwap(I);
		}
	}
}

void UJellyDeformerComponent::ApplyEngulfWrap()
{
	if (!Settings.bUndersideEngulf || SmoothedEngulfs.Num() == 0)
	{
		return;
	}

	for (const FEngulfSite& E : SmoothedEngulfs)
	{
		if (E.Strength < 0.02f || E.RadiusCm < 1.f)
		{
			continue;
		}
		// Obvious cavity: at least EngulfMinWrapCm, scales with obstacle height.
		const float WrapCm = FMath::Max(Settings.EngulfMinWrapCm, E.HeightCm * Settings.EngulfWrapStrength) * E.Strength;
		const FVector2D TopXY(E.LocalTop.X, E.LocalTop.Y);
		const float TopZ = E.LocalTop.Z;

		for (int32 I = 0; I < Rest.Num(); ++I)
		{
			// Wrap lower + mid band (rocks poke through equator on Hunt).
			if (Rest[I].Z > RestRadiusCm() * 0.55f)
			{
				continue;
			}
			const FVector2D RXY(Rest[I].X, Rest[I].Y);
			const float Dist = (RXY - TopXY).Size();
			if (Dist > E.RadiusCm)
			{
				continue;
			}
			const float T = 1.f - Dist / E.RadiusCm;
			float Inf = SmoothStep(T);
			const float Under = SmoothStep(FMath::Clamp(
				(RestRadiusCm() * 0.55f - Rest[I].Z) / (RestRadiusCm() * 1.1f), 0.f, 1.f));
			Inf *= FMath::Lerp(0.45f, 1.f, Under);
			if (Inf < 0.001f)
			{
				continue;
			}

			// Force verts above the obstacle top — cavity so the rock sits under the goo.
			const float TargetZ = TopZ + WrapCm * 0.55f;
			const float Lift = FMath::Max(0.f, TargetZ - Rest[I].Z) * Inf;
			const float Cave = FMath::Min(Lift, WrapCm) ;
			Pose[I].Z += Cave;

			FVector2D Away = RXY - TopXY;
			Away = Away.SizeSquared() < KINDA_SMALL_NUMBER ? RXY.GetSafeNormal() : Away.GetSafeNormal();
			const float Ooze = Cave * Settings.EngulfLateralOoze;
			Pose[I].X += Away.X * Ooze;
			Pose[I].Y += Away.Y * Ooze;
			if (Dist > E.RadiusCm * 0.4f)
			{
				Pose[I].Z -= Ooze * 0.2f * Inf;
			}

			Influence[I] = FMath::Max(Influence[I], Inf * E.Strength);
			// Bypass slow viscosity creep so wrap is immediately readable on Prey_Rock.
			Current[I] = FMath::Lerp(Current[I], Pose[I], 0.72f);
		}
	}
	ClampPose();
	ClampCurrentToRest();
}

void UJellyDeformerComponent::UpdateSmoothedContacts(float Dt)
{
	// Match pending targets into smoothed slots (by normal proximity).
	for (const FPendingContact& P : PendingContacts)
	{
		int32 Best = INDEX_NONE;
		float BestDot = 0.55f;
		for (int32 I = 0; I < SmoothedContacts.Num(); ++I)
		{
			const float D = FVector::DotProduct(SmoothedContacts[I].LocalNormal, P.LocalNormal);
			if (D > BestDot)
			{
				BestDot = D;
				Best = I;
			}
		}
		if (Best == INDEX_NONE)
		{
			if (SmoothedContacts.Num() >= MaxContactsPerStep)
			{
				continue;
			}
			FSmoothedContact S;
			S.LocalPoint = P.LocalPoint;
			S.LocalNormal = P.LocalNormal;
			S.TargetPressureCm = P.TargetPressureCm;
			S.PressureCm = P.TargetPressureCm * 0.35f;
			S.RadiusCm = P.RadiusCm;
			S.IntoSpeedCms = P.IntoSpeedCms;
			S.Age = 0.f;
			SmoothedContacts.Add(S);
		}
		else
		{
			FSmoothedContact& S = SmoothedContacts[Best];
			S.LocalPoint = FMath::Lerp(S.LocalPoint, P.LocalPoint, 0.45f);
			S.LocalNormal = (S.LocalNormal + P.LocalNormal).GetSafeNormal();
			S.TargetPressureCm = P.TargetPressureCm;
			S.RadiusCm = FMath::Lerp(S.RadiusCm, P.RadiusCm, 0.35f);
			S.IntoSpeedCms = P.IntoSpeedCms;
			S.Age = 0.f;
		}
	}

	const float Alpha = 1.f - FMath::Exp(-FMath::Max(0.5f, Settings.PressureSmoothRate) * Dt);
	for (int32 I = SmoothedContacts.Num() - 1; I >= 0; --I)
	{
		FSmoothedContact& S = SmoothedContacts[I];
		S.Age += Dt;
		// Decay target when not refreshed this frame.
		if (S.Age > Dt * 1.5f)
		{
			S.TargetPressureCm = FMath::Lerp(S.TargetPressureCm, 0.f, Alpha);
		}
		S.PressureCm = FMath::Lerp(S.PressureCm, S.TargetPressureCm, Alpha);
		if (S.PressureCm < 0.4f && S.TargetPressureCm < 0.4f && S.Age > 0.12f)
		{
			SmoothedContacts.RemoveAtSwap(I);
		}
	}
}

void UJellyDeformerComponent::ApplyDirectionalFlow(const FVector& LocalFlowDir, float FlowAmount, float StepUpCm)
{
	if (!Settings.bDirectionalFlow || FlowAmount < 0.02f || LocalFlowDir.IsNearlyZero())
	{
		return;
	}

	const FVector Flow = LocalFlowDir.GetSafeNormal();
	FVector Side = FVector::CrossProduct(Flow, FVector::UpVector).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}
	const float SquashCm = FlowAmount * Settings.FlowSquash * 18.f;
	const float StretchCm = FlowAmount * Settings.FlowStretch * 14.f;
	const float SpreadCm = FlowAmount * Settings.TangentialSpread * 10.f;
	const float StepSquashCm = FMath::Min(StepUpCm * Settings.StepUpSquash * 0.35f, Settings.MaxContactDeformationM * 80.f);

	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const FVector RN = Rest[I].GetSafeNormal();
		const float Face = FVector::DotProduct(RN, Flow);
		FVector Off = FVector::ZeroVector;
		if (Face > 0.05f)
		{
			// Leading face compresses against travel (fluid push).
			Off -= Flow * (SquashCm * Face);
			Off += Side * (SpreadCm * Face * FMath::Sign(FVector::DotProduct(RN, Side)));
		}
		else if (Face < -0.05f)
		{
			// Trailing face elongates along travel.
			Off -= Flow * (StretchCm * Face); // Face negative => +Flow
		}

		// Climbing a ledge: lower hemisphere flattens / spreads while rising.
		if (StepSquashCm > 0.f && Rest[I].Z < 0.f)
		{
			const float Low = FMath::Clamp(-Rest[I].Z / (RestRadiusCm() * 0.92f), 0.f, 1.f);
			Off.Z += StepSquashCm * Low;
			const float R = FVector2D(Rest[I].X, Rest[I].Y).Size();
			if (R > KINDA_SMALL_NUMBER)
			{
				Off.X += Rest[I].X / R * StepSquashCm * 0.45f * Low;
				Off.Y += Rest[I].Y / R * StepSquashCm * 0.45f * Low;
			}
		}

		Pose[I] += Off;
		Influence[I] = FMath::Max(Influence[I], FMath::Abs(Face) * FlowAmount);
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplySmoothedContacts()
{
	if (!Settings.bContactDeformation || bAscending || SmoothedContacts.Num() == 0)
	{
		return;
	}

	const float InvSqrt = 1.f / FMath::Sqrt(static_cast<float>(SmoothedContacts.Num()));
	for (const FSmoothedContact& C : SmoothedContacts)
	{
		const FVector Dent = C.LocalNormal.GetSafeNormal();
		if (Dent.IsNearlyZero() || C.PressureCm < 0.35f)
		{
			continue;
		}
		const float PressCm = C.PressureCm * InvSqrt;
		FVector Tangent = FVector::VectorPlaneProject(SmoothedFlowDir, Dent).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(Dent, FVector::UpVector).GetSafeNormal();
		}

		for (int32 I = 0; I < Rest.Num(); ++I)
		{
			const float Facing = FVector::DotProduct(Rest[I].GetSafeNormal(), -Dent);
			if (Facing < 0.08f)
			{
				continue;
			}
			const float Dist = FVector::Dist(Rest[I], C.LocalPoint);
			if (Dist > C.RadiusCm)
			{
				continue;
			}
			const float T = 1.f - Dist / C.RadiusCm;
			float Inf = SmoothStep(T) * FMath::Clamp((Facing - 0.08f) / 0.72f, 0.f, 1.f);
			if (!FMath::IsNearlyEqual(Settings.ContactFalloff, 1.f))
			{
				Inf = FMath::Pow(Inf, FMath::Max(0.2f, Settings.ContactFalloff));
			}
			if (Inf < 0.001f)
			{
				continue;
			}
			Influence[I] = FMath::Max(Influence[I], Inf);
			// Squash into obstacle + slight tangential goo spread along move.
			Pose[I] += Dent * (PressCm * Inf);
			if (!Tangent.IsNearlyZero())
			{
				Pose[I] += Tangent * (PressCm * Inf * Settings.TangentialSpread * 0.35f);
			}
			// Opposite hemisphere gently stretches with flow (fluid continuity).
			if (Facing < 0.35f && !SmoothedFlowDir.IsNearlyZero())
			{
				Pose[I] += SmoothedFlowDir * (PressCm * Inf * Settings.FlowStretch * 0.15f);
			}
		}
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplyGround(float Dt, bool bGrounded, float GroundImpactSpeedMps)
{
	const float MaxLand = Settings.MaxLandingDeformationM;
	if (!Settings.bGroundContact || !bGrounded)
	{
		const float Pull = 1.f - FMath::Exp(-5.5f * Dt);
		LandingPress = FMath::Lerp(LandingPress, 0.f, Pull);
		LandingVelocity = FMath::Lerp(LandingVelocity, 0.f, Pull);
		bWasGrounded = false;
		return;
	}

	if (Settings.bLandingImpact && !bWasGrounded && GroundImpactSpeedMps >= Settings.LandingVelocityThresholdMps)
	{
		// Instant flatten on impact — almost no rebound impulse (viscous liquid).
		const float Peak = FMath::Min(GroundImpactSpeedMps * Settings.LandingImpactStrength, MaxLand);
		LandingPress = FMath::Max(LandingPress, Peak);
		LandingVelocity = Peak * Settings.LandingBounceStrength; // ~0 ⇒ no spring pop
	}
	bWasGrounded = true;

	{
		// Overdamped / exponential settle toward steady ground sit — no oscillation.
		const float Rate = FMath::Max(0.5f, Settings.LandingRecoverySpeed);
		const float Alpha = 1.f - FMath::Exp(-Rate * Dt);
		const float TargetSit = Settings.GroundContactStrengthM * 0.35f;
		LandingPress = FMath::Lerp(LandingPress, TargetSit, Alpha);
		LandingVelocity = FMath::Lerp(LandingVelocity, 0.f, Alpha);
		LandingPress = FMath::Clamp(LandingPress, 0.f, MaxLand);
	}

	float CompressionM = Settings.GroundContactStrengthM + LandingPress;
	CompressionM = FMath::Clamp(CompressionM, -MaxLand * 0.3f, Settings.MaxGroundCompressionM + MaxLand);

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
		if (!FMath::IsNearlyEqual(Settings.GroundContactFalloff, 1.f))
		{
			Inf = FMath::Pow(Inf, FMath::Max(0.2f, Settings.GroundContactFalloff));
		}
		if (Inf < 0.001f)
		{
			continue;
		}
		Influence[I] = FMath::Max(Influence[I], Inf);
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
	// Slow lateral ooze — don't slam volume correction in one frame.
	const float Ooze = FMath::Clamp(Settings.VolumePreservation * 0.55f, 0.f, 1.f);
	RadialScale = FMath::Lerp(1.f, RadialScale, Ooze);

	const float MaxExpandCm = Settings.MaxRadialExpansionM * 100.f;
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		FVector Want = Pose[I];
		FVector2D Horiz(Want.X, Want.Y);
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
			Want.X = Horiz.X;
			Want.Y = Horiz.Y;
		}
		const float MaxVertCm = Settings.MaxVerticalCompressionM * 100.f;
		const float ZOff = Want.Z - Rest[I].Z;
		Want.Z = Rest[I].Z + FMath::Clamp(ZOff, -MaxVertCm, MaxVerticalStretchCm());
		Pose[I] = Want;
	}
	ClampPose();
}

void UJellyDeformerComponent::ApplyInertia(const FVector& LocalAccelCms)
{
	// Thick liquid barely stores elastic kick — only a whisper of lag.
	const FVector Kick = -LocalAccelCms * Settings.MovementDeformation * 0.12f;
	if (Kick.SizeSquared() < 100.f)
	{
		return;
	}
	const FVector KickDir = Kick.GetSafeNormal();
	for (int32 I = 0; I < Rest.Num(); ++I)
	{
		const float Face = FVector::DotProduct(Rest[I].GetSafeNormal(), KickDir);
		const float W = FMath::Lerp(0.25f, 1.f, FMath::Clamp((Face + 0.5f), 0.f, 1.f));
		Velocity[I] += Kick * (W * Settings.StretchStrength * 0.18f);
	}
}

void UJellyDeformerComponent::StepSprings(float Dt)
{
	ClampCurrentToRest();
	const float MaxD = MaxDeformCm();
	const float MaxSpeed = MaxD * 4.5f; // kill fast elastic snaps

	// Primary: exponential creep toward pose (no oscillation).
	const float Creep = 1.f - FMath::Exp(-FMath::Max(0.4f, Settings.ViscosityCreepRate) * Dt);
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		Current[I] = FMath::Lerp(Current[I], Pose[I], Creep);
	}

	// Residual overdamped spring (ζ = DampingRatio ≥ 1) — languid settle, not bounce.
	const float K = FMath::Max(0.01f, Settings.SpringStrength * Settings.RecoverySpeed);
	const float Omega = FMath::Sqrt(K);
	const float CritDamp = 2.f * Omega;
	const float Damp = FMath::Max(Settings.Damping, CritDamp * FMath::Max(1.f, Settings.DampingRatio));

	TArray<FVector> Force;
	Force.SetNumZeroed(Current.Num());
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		Force[I] += -K * (Current[I] - Pose[I]) - Damp * Velocity[I];
	}

	// Soft neighbor coupling (low strength → fewer ringing modes).
	for (const FEdge& E : Edges)
	{
		const FVector CurDelta = Current[E.B] - Current[E.A];
		const FVector PoseDelta = Pose[E.B] - Pose[E.A];
		const FVector Term = (CurDelta - PoseDelta) * Settings.NeighborStrength;
		Force[E.A] += Term;
		Force[E.B] -= Term;
	}

	const float VelKill = 1.f - FMath::Exp(-FMath::Max(4.f, Settings.Damping * 0.65f) * Dt);
	for (int32 I = 0; I < Current.Num(); ++I)
	{
		Velocity[I] += Force[I] * Dt;
		Velocity[I] = FMath::Lerp(Velocity[I], FVector::ZeroVector, VelKill);
		if (Velocity[I].Size() > MaxSpeed)
		{
			Velocity[I] = Velocity[I].GetSafeNormal() * MaxSpeed;
		}
		Current[I] += Velocity[I] * Dt;
	}

	// Gentle length limits — partial correction so edges ooze, not snap.
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
			const FVector Corr = Delta.GetSafeNormal() * ((Len - Target) * 0.22f);
			Current[E.A] += Corr;
			Current[E.B] -= Corr;
		}
	}
}

void UJellyDeformerComponent::Step(float Dt, const FVector& WorldVelocityCms, const FVector& WorldAccelCms, bool bGrounded, float GroundImpactSpeedMps,
	const FVector& WorldMoveIntentCms, float StepUpCm)
{
	if (!bReady)
	{
		return;
	}

	AActor* Owner = GetOwner();
	const FRotator YawRot(0.f, Owner ? Owner->GetActorRotation().Yaw : 0.f, 0.f);
	const FVector LocalVel = YawRot.UnrotateVector(WorldVelocityCms);
	const FVector LocalAcc = YawRot.UnrotateVector(WorldAccelCms);
	bAscending = LocalVel.Z > 180.f;

	// Blend move intent with actual velocity for fluid direction.
	FVector FlowWorld = WorldMoveIntentCms.SizeSquared() > WorldVelocityCms.SizeSquared() * 0.25f
		? WorldMoveIntentCms
		: FVector(WorldVelocityCms.X, WorldVelocityCms.Y, 0.f);
	FlowWorld.Z = 0.f;
	const FVector LocalFlow = YawRot.UnrotateVector(FlowWorld);
	const float TargetFlow = FMath::Clamp(LocalFlow.Size() * 0.01f * 0.2f, 0.f, 1.f);
	// Slow flow-field blend — pressure creeps with move intent, doesn't whip.
	const float FlowAlpha = 1.f - FMath::Exp(-3.2f * Dt);
	SmoothedFlowAmount = FMath::Lerp(SmoothedFlowAmount, TargetFlow, FlowAlpha);
	if (!LocalFlow.IsNearlyZero())
	{
		const FVector WantDir = LocalFlow.GetSafeNormal();
		SmoothedFlowDir = (SmoothedFlowDir.IsNearlyZero() ? WantDir : FMath::Lerp(SmoothedFlowDir, WantDir, FlowAlpha).GetSafeNormal());
	}

	GatherWorldContacts(WorldVelocityCms);
	GatherEngulfObstacles();
	UpdateSmoothedContacts(Dt);
	UpdateSmoothedEngulfs(Dt, StepUpCm);

	BuildPose(LocalVel, LocalAcc);
	ApplyDirectionalFlow(SmoothedFlowDir, SmoothedFlowAmount, StepUpCm);
	ApplySmoothedContacts();
	ApplyEngulfWrap();
	ApplyGround(Dt, bGrounded, GroundImpactSpeedMps);
	ApplyVolumePreservation();
	ApplyInertia(LocalAcc);
	StepSprings(Dt);
	// Soft-volume drape after springs so particle support heights win the silhouette.
	ApplySoftVolumeSkin();
	// Re-clamp after soft skin so airborne particle lag can't needle the shell.
	ClampCurrentToRest();
	WriteMesh();

	PrevWorldVelocityCms = WorldVelocityCms;
}

void UJellyDeformerComponent::ResetMotion()
{
	LandingPress = 0.f;
	LandingVelocity = 0.f;
	SmoothedFlowAmount = 0.f;
	SmoothedFlowDir = FVector::ZeroVector;
	bWasGrounded = false;
	PendingContacts.Reset();
	SmoothedContacts.Reset();
	PendingEngulfs.Reset();
	SmoothedEngulfs.Reset();
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
