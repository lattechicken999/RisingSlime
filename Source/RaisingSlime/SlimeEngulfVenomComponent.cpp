#include "SlimeEngulfVenomComponent.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "Devourable.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SceneComponent.h"

USlimeEngulfVenomComponent::USlimeEngulfVenomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void USlimeEngulfVenomComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureMeshes();
}

void USlimeEngulfVenomComponent::EnsureMeshes()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const int32 Want = FMath::Clamp(RibbonCount, 1, 3);
	while (Ribbons.Num() < Want)
	{
		const FName Name(*FString::Printf(TEXT("EngulfVenomRibbon_%d"), Ribbons.Num()));
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner, Name);
		Mesh->SetupAttachment(Owner->GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(false);
		Mesh->RegisterComponent();
		FRibbonMesh R;
		R.Mesh = Mesh;
		Ribbons.Add(R);
	}
}

void USlimeEngulfVenomComponent::BeginEngulf(AActor* InPrey, UMaterialInterface* SlimeMaterial)
{
	if (!InPrey)
	{
		Clear();
		return;
	}

	EnsureMeshes();
	Prey = InPrey;
	Material = SlimeMaterial;
	Target01 = 0.f;
	Visual01 = 0.f;
	Fade01 = 0.f;
	bActive = true;
	bFading = false;
	RefreshPreyBounds();
	RebuildAllRibbons();

	UE_LOG(LogTemp, Display, TEXT("EngulfVenom: begin wrap prey=%s r=%.1f h=%.1f"),
		*InPrey->GetName(), PreyRadiusXY, PreyTopZ - PreyBottomZ);
}

void USlimeEngulfVenomComponent::SetEngulfProgress01(float Engulf01)
{
	Target01 = FMath::Clamp(Engulf01, 0.f, 1.f);
}

void USlimeEngulfVenomComponent::BeginFadeOut()
{
	if (!bActive)
	{
		return;
	}
	bFading = true;
	UE_LOG(LogTemp, Display, TEXT("EngulfVenom: fade-out (visual=%.2f)"), Visual01);
}

void USlimeEngulfVenomComponent::Clear()
{
	bActive = false;
	bFading = false;
	Target01 = 0.f;
	Visual01 = 0.f;
	Fade01 = 0.f;
	Prey = nullptr;
	for (FRibbonMesh& R : Ribbons)
	{
		if (R.Mesh)
		{
			R.Mesh->ClearAllMeshSections();
			R.Mesh->SetVisibility(false);
		}
	}
}

void USlimeEngulfVenomComponent::RefreshPreyBounds()
{
	AActor* P = Prey.Get();
	if (!P)
	{
		return;
	}

	FVector Origin = P->GetActorLocation();
	FVector Extent(25.f, 25.f, 25.f);
	if (const ADevourable* Dev = Cast<ADevourable>(P))
	{
		if (Dev->MeshComponent)
		{
			const FBoxSphereBounds B = Dev->MeshComponent->Bounds;
			Origin = B.Origin;
			Extent = B.BoxExtent;
		}
	}
	else if (USceneComponent* Root = P->GetRootComponent())
	{
		const FBoxSphereBounds B = Root->Bounds;
		Origin = B.Origin;
		Extent = B.BoxExtent;
	}

	PreyCenter = Origin;
	PreyRadiusXY = FMath::Max(16.f, FMath::Max(Extent.X, Extent.Y));
	PreyBottomZ = Origin.Z - Extent.Z;
	PreyTopZ = Origin.Z + Extent.Z;
	if (PreyTopZ <= PreyBottomZ + 1.f)
	{
		PreyTopZ = PreyBottomZ + 40.f;
	}
}

FVector USlimeEngulfVenomComponent::GetBodyAttachWorld() const
{
	const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner());
	FVector Body = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	float BodyR = 50.f;
	if (Pawn)
	{
		if (const USlimeSoftVolumeComponent* Soft = Pawn->GetSoftVolume())
		{
			Body = Soft->GetBodyCenterWorld();
			BodyR = Soft->GetBodyRadiusCm();
		}
	}

	FVector ToPrey = PreyCenter - Body;
	ToPrey.Z = 0.f;
	if (ToPrey.SizeSquared() < 1.f)
	{
		ToPrey = FVector(1.f, 0.f, 0.f);
	}
	ToPrey.Normalize();
	FVector Attach = Body + ToPrey * (BodyR * 0.72f);
	Attach.Z = FMath::Lerp(Body.Z - BodyR * 0.35f, PreyBottomZ + 8.f, 0.55f);
	return Attach;
}

void USlimeEngulfVenomComponent::BuildRibbonSection(int32 RibbonIndex, float Wrap01, float Opacity)
{
	if (!Ribbons.IsValidIndex(RibbonIndex) || !Ribbons[RibbonIndex].Mesh)
	{
		return;
	}

	UProceduralMeshComponent* Mesh = Ribbons[RibbonIndex].Mesh;
	const int32 PathN = 18;
	const int32 RingN = 7;
	const float BaseAngle = (UE_TWO_PI * static_cast<float>(RibbonIndex)) / static_cast<float>(FMath::Max(1, RibbonCount));
	const float WrapAngle = FMath::Lerp(0.55f, 2.15f, Wrap01) * PI; // wraps around sides → crown
	const float ReachZ = FMath::Lerp(PreyBottomZ + 10.f, PreyTopZ + 4.f, Wrap01);
	const FVector BodyAttach = GetBodyAttachWorld();

	TArray<FVector> Path;
	Path.Reserve(PathN);
	for (int32 I = 0; I < PathN; ++I)
	{
		const float T = static_cast<float>(I) / static_cast<float>(PathN - 1);
		FVector Pos;
		if (T < 0.22f)
		{
			const float U = T / 0.22f;
			const float Ease = U * U * (3.f - 2.f * U);
			FVector PreyBase = PreyCenter + FVector(FMath::Cos(BaseAngle), FMath::Sin(BaseAngle), 0.f) * (PreyRadiusXY + CoatExtraCm);
			PreyBase.Z = PreyBottomZ + 8.f;
			Pos = FMath::Lerp(BodyAttach, PreyBase, Ease);
		}
		else
		{
			const float U = (T - 0.22f) / 0.78f;
			const float Angle = BaseAngle + U * WrapAngle;
			const float H = FMath::Lerp(PreyBottomZ + 8.f, ReachZ, U * U * (3.f - 2.f * U));
			const float Coat = CoatExtraCm * FMath::Lerp(1.15f, 0.55f, U);
			Pos = PreyCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (PreyRadiusXY + Coat);
			Pos.Z = H;
		}
		Path.Add(Pos);
	}

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UV;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> Colors;
	Verts.Reserve(PathN * RingN);
	Normals.Reserve(PathN * RingN);
	UV.Reserve(PathN * RingN);
	Colors.Reserve(PathN * RingN);

	for (int32 I = 0; I < PathN; ++I)
	{
		const float T = static_cast<float>(I) / static_cast<float>(PathN - 1);
		FVector Tangent = FVector::ForwardVector;
		if (I + 1 < PathN)
		{
			Tangent = (Path[I + 1] - Path[I]).GetSafeNormal();
		}
		else if (I > 0)
		{
			Tangent = (Path[I] - Path[I - 1]).GetSafeNormal();
		}
		FVector Normal = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::CrossProduct(Tangent, FVector::RightVector).GetSafeNormal();
		}
		const FVector Binormal = FVector::CrossProduct(Tangent, Normal).GetSafeNormal();

		// Lobes: thicker near body, taper at tip.
		const float Rad = RibbonRadiusCm * FMath::Lerp(1.35f, 0.45f, T) * FMath::Lerp(0.55f, 1.f, Wrap01);
		for (int32 J = 0; J < RingN; ++J)
		{
			const float A = UE_TWO_PI * static_cast<float>(J) / static_cast<float>(RingN);
			const FVector Offset = (Normal * FMath::Cos(A) + Binormal * FMath::Sin(A)) * Rad;
			Verts.Add(Path[I] + Offset);
			Normals.Add(Offset.GetSafeNormal());
			UV.Add(FVector2D(T, static_cast<float>(J) / static_cast<float>(RingN)));
			Colors.Add(FLinearColor(1.f, 1.f, 1.f, Opacity));
			Tangents.Add(FProcMeshTangent(Tangent, false));
		}
	}

	for (int32 I = 0; I < PathN - 1; ++I)
	{
		for (int32 J = 0; J < RingN; ++J)
		{
			const int32 J1 = (J + 1) % RingN;
			const int32 A = I * RingN + J;
			const int32 B = I * RingN + J1;
			const int32 C = (I + 1) * RingN + J;
			const int32 D = (I + 1) * RingN + J1;
			Tris.Append({A, C, B});
			Tris.Append({B, C, D});
		}
	}

	// Path points are world-space; convert to mesh local (attached to owner root).
	const FTransform Xf = Mesh->GetComponentTransform();
	for (FVector& V : Verts)
	{
		V = Xf.InverseTransformPosition(V);
	}
	for (FVector& N : Normals)
	{
		N = Xf.InverseTransformVectorNoScale(N).GetSafeNormal();
	}

	if (Mesh->GetNumSections() == 0)
	{
		Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV, Colors, Tangents, false);
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, Verts, Normals, UV, Colors, Tangents);
	}

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
		if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			Mid->SetScalarParameterValue(TEXT("Opacity_Appearance"), Opacity);
		}
	}

	Mesh->SetVisibility(Opacity > 0.04f && Wrap01 > 0.02f);
}

void USlimeEngulfVenomComponent::RebuildAllRibbons()
{
	const float Opacity = FMath::Clamp(1.f - Fade01, 0.f, 1.f) * FMath::Clamp(Visual01 * 1.15f, 0.15f, 1.f);
	const int32 N = FMath::Clamp(RibbonCount, 1, Ribbons.Num());
	for (int32 I = 0; I < N; ++I)
	{
		BuildRibbonSection(I, Visual01, Opacity);
	}
	for (int32 I = N; I < Ribbons.Num(); ++I)
	{
		if (Ribbons[I].Mesh)
		{
			Ribbons[I].Mesh->SetVisibility(false);
		}
	}
}

void USlimeEngulfVenomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive)
	{
		return;
	}

	if (!Prey.IsValid())
	{
		Clear();
		return;
	}

	RefreshPreyBounds();

	const float Follow = 1.f - FMath::Exp(-FMath::Max(0.5f, VisualFollowRate) * DeltaTime);
	Visual01 = FMath::Lerp(Visual01, Target01, Follow);

	if (bFading)
	{
		Fade01 = FMath::Clamp(Fade01 + DeltaTime / FMath::Max(0.08f, FadeOutSeconds), 0.f, 1.f);
		if (Fade01 >= 1.f)
		{
			Clear();
			return;
		}
	}

	RebuildAllRibbons();
}
