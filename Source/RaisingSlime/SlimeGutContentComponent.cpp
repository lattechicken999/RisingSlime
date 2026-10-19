#include "SlimeGutContentComponent.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "RaisingSlimeTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

USlimeGutContentComponent::USlimeGutContentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

FVector USlimeGutContentComponent::GutAnchorLocal() const
{
	return FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm);
}

void USlimeGutContentComponent::Swallow(ADevourable* Prey)
{
	if (!Prey || !Prey->MeshComponent || !Prey->MeshComponent->GetStaticMesh() || !GetOwner())
	{
		return;
	}

	UStaticMeshComponent* Proxy = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
	if (!Proxy)
	{
		return;
	}

	Proxy->SetupAttachment(GetOwner()->GetRootComponent());
	Proxy->RegisterComponent();
	Proxy->SetStaticMesh(Prey->MeshComponent->GetStaticMesh());
	Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Proxy->SetGenerateOverlapEvents(false);
	Proxy->SetCastShadow(false);
	Proxy->SetMobility(EComponentMobility::Movable);
	Proxy->bRenderableInSceneCapture = true;

	// Copy materials so rock/creature look matches world prey.
	const int32 Mats = Prey->MeshComponent->GetNumMaterials();
	for (int32 I = 0; I < Mats; ++I)
	{
		Proxy->SetMaterial(I, Prey->MeshComponent->GetMaterial(I));
	}

	const FVector PreyScale = Prey->GetActorScale3D();
	const float Uniform = FMath::Max(PreyScale.GetAbsMax(), 0.2f) * FitScale;
	Proxy->SetRelativeScale3D(FVector(Uniform));
	Proxy->SetRelativeRotation(FRotator(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-8.f, 8.f)));
	Proxy->SetVisibility(true);
	Proxy->SetHiddenInGame(false);

	FGutEntry Entry;
	Entry.Mesh = Proxy;
	Entry.BaseLocal = GutAnchorLocal() + FVector(
		FMath::FRandRange(-10.f, 10.f),
		FMath::FRandRange(-10.f, 10.f),
		FMath::FRandRange(-6.f, 8.f));
	// Pull from prey world position into the body center.
	if (USceneComponent* Root = GetOwner()->GetRootComponent())
	{
		Entry.PullStartLocal = Root->GetComponentTransform().InverseTransformPosition(Prey->GetActorLocation());
	}
	else
	{
		Entry.PullStartLocal = Entry.BaseLocal;
	}
	Proxy->SetRelativeLocation(Entry.PullStartLocal);
	Entry.Phase = FMath::FRandRange(0.f, PI * 2.f);
	Entry.RestScale = Uniform;
	Entry.bPulling = true;
	Entry.Pull01 = 0.f;
	Entries.Add(Entry);

	SwellBonus = FMath::Min(SwellBonus + SwallowSwell, 0.45f);

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: swallowed %s (gut=%d swell=+%.2f)"),
		*Prey->GetLabel(), Entries.Num(), SwellBonus);
}

void USlimeGutContentComponent::BeginDissolveAll()
{
	for (FGutEntry& E : Entries)
	{
		E.bDissolving = true;
	}
	if (Entries.Num() > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("SlimeGut: dissolving %d contents"), Entries.Num());
	}
}

void USlimeGutContentComponent::ClearImmediate()
{
	for (FGutEntry& E : Entries)
	{
		if (E.Mesh)
		{
			E.Mesh->DestroyComponent();
			E.Mesh = nullptr;
		}
	}
	Entries.Reset();
	SwellBonus = 0.f;
}

void USlimeGutContentComponent::TickEntry(FGutEntry& Entry, float Dt)
{
	if (!Entry.Mesh)
	{
		return;
	}

	Entry.Phase += Dt * BobSpeed;
	const FVector Bob(
		FMath::Sin(Entry.Phase) * BobAmpCm * 0.35f,
		FMath::Cos(Entry.Phase * 0.85f) * BobAmpCm * 0.35f,
		FMath::Sin(Entry.Phase * 1.3f) * BobAmpCm);

	if (Entry.bPulling)
	{
		Entry.Pull01 = FMath::Clamp(Entry.Pull01 + Dt / FMath::Max(0.1f, PullInSeconds), 0.f, 1.f);
		const float T = Entry.Pull01 * Entry.Pull01 * (3.f - 2.f * Entry.Pull01);
		const FVector Target = Entry.BaseLocal + Bob * T;
		Entry.Mesh->SetRelativeLocation(FMath::Lerp(Entry.PullStartLocal, Target, T));
		if (Entry.Pull01 >= 1.f)
		{
			Entry.bPulling = false;
		}
	}
	else
	{
		Entry.Mesh->SetRelativeLocation(Entry.BaseLocal + Bob);
		Entry.Mesh->AddLocalRotation(FRotator(0.f, Dt * 18.f, 0.f));
	}

	if (Entry.bDissolving)
	{
		Entry.Dissolve01 = FMath::Clamp(Entry.Dissolve01 + Dt / FMath::Max(0.2f, DissolveSeconds), 0.f, 1.f);
		const float Remain = 1.f - Entry.Dissolve01;
		const float S = Entry.RestScale * FMath::Max(0.02f, Remain * Remain);
		Entry.Mesh->SetRelativeScale3D(FVector(S));
		Entry.Mesh->SetVisibility(Remain > 0.04f);
		// Soft fade via scalar opacity if MIDs exist; otherwise scale melt is enough.
		for (int32 I = 0; I < Entry.Mesh->GetNumMaterials(); ++I)
		{
			if (UMaterialInstanceDynamic* Mid = Entry.Mesh->CreateAndSetMaterialInstanceDynamic(I))
			{
				Mid->SetScalarParameterValue(TEXT("Opacity"), Remain);
				Mid->SetScalarParameterValue(TEXT("Opacity_Appearance"), Remain);
			}
		}
	}
}

void USlimeGutContentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SwellBonus > 0.f)
	{
		SwellBonus = FMath::Max(0.f, SwellBonus - SwellDecayPerSec * DeltaTime);
	}

	for (int32 I = Entries.Num() - 1; I >= 0; --I)
	{
		TickEntry(Entries[I], DeltaTime);
		if (Entries[I].bDissolving && Entries[I].Dissolve01 >= 1.f)
		{
			if (Entries[I].Mesh)
			{
				Entries[I].Mesh->DestroyComponent();
			}
			Entries.RemoveAtSwap(I);
		}
	}
}
