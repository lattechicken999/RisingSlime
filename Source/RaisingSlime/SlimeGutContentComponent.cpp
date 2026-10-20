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

float USlimeGutContentComponent::GetContainedMass() const
{
	float Total = 0.f;
	for (const FGutEntry& E : Entries)
	{
		if (!E.bDissolving)
		{
			Total += E.Mass;
		}
		else
		{
			Total += E.Mass * FMath::Max(0.f, 1.f - E.Dissolve01);
		}
	}
	return Total;
}

float USlimeGutContentComponent::GetSwallowSwellBonus() const
{
	return SwellBonus + GetContainedMass() * ContainedMassToSize;
}

void USlimeGutContentComponent::Swallow(ADevourable* Prey)
{
	if (!Prey || !GetOwner())
	{
		return;
	}

	USceneComponent* Root = GetOwner()->GetRootComponent();
	if (!Root)
	{
		return;
	}

	const FVector WorldLoc = Prey->GetActorLocation();
	const FVector WorldScale = Prey->GetActorScale3D();

	Prey->AttachIntoGut(Root);

	FGutEntry Entry;
	Entry.Prey = Prey;
	Entry.Mass = Prey->Mass;
	Entry.Trait = Prey->Trait;
	Entry.AbsorbRemaining = FMath::Max(0.5f, AbsorbDuration);
	Entry.BaseLocal = GutAnchorLocal() + FVector(
		FMath::FRandRange(-12.f, 12.f),
		FMath::FRandRange(-12.f, 12.f),
		FMath::FRandRange(-8.f, 10.f));
	Entry.PullStartLocal = Root->GetComponentTransform().InverseTransformPosition(WorldLoc);
	Entry.RestScale3D = WorldScale * FitScale;
	Entry.Phase = FMath::FRandRange(0.f, PI * 2.f);
	Entry.bPulling = true;
	Entry.Pull01 = 0.f;

	Prey->SetActorRelativeLocation(Entry.PullStartLocal);
	Prey->SetActorRelativeScale3D(Entry.RestScale3D);
	Prey->SetActorRelativeRotation(FRotator(
		FMath::FRandRange(-12.f, 12.f),
		FMath::FRandRange(0.f, 360.f),
		FMath::FRandRange(-8.f, 8.f)));

	Entries.Add(Entry);
	SwellBonus = FMath::Min(SwellBonus + SwallowSwell + Prey->Mass * 0.04f, 0.7f);

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: swallowed %s mass=%.1f (visible attach, absorb=%.1fs, gut=%d)"),
		*Prey->GetLabel(), Entry.Mass, Entry.AbsorbRemaining, Entries.Num());
}

void USlimeGutContentComponent::BeginDissolveAll()
{
	for (FGutEntry& E : Entries)
	{
		E.bDissolving = true;
		E.bAbsorbNotified = true;
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
		FinishEntry(E);
	}
	Entries.Reset();
	SwellBonus = 0.f;
}

void USlimeGutContentComponent::FinishEntry(FGutEntry& Entry)
{
	if (ADevourable* Prey = Entry.Prey.Get())
	{
		Prey->FinishGutAndRespawn();
	}
	Entry.Prey = nullptr;
}

void USlimeGutContentComponent::TickEntry(FGutEntry& Entry, float Dt)
{
	ADevourable* Prey = Entry.Prey.Get();
	if (!Prey)
	{
		return;
	}

	if (!Entry.bDissolving && !Entry.bAbsorbNotified)
	{
		Entry.AbsorbRemaining -= Dt;
		if (Entry.AbsorbRemaining <= 0.f)
		{
			Entry.AbsorbRemaining = 0.f;
			Entry.bAbsorbNotified = true;
			Entry.bDissolving = true;
			OnGutAbsorbed.ExecuteIfBound(Entry.Mass, Entry.Trait, Prey);
			UE_LOG(LogTemp, Display, TEXT("SlimeGut: absorb complete mass=%.1f trait=%s — dissolving"),
				Entry.Mass, *TraitText::Name(Entry.Trait));
		}
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
		Prey->SetActorRelativeLocation(FMath::Lerp(Entry.PullStartLocal, Target, T));
		if (Entry.Pull01 >= 1.f)
		{
			Entry.bPulling = false;
		}
	}
	else
	{
		Prey->SetActorRelativeLocation(Entry.BaseLocal + Bob);
		Prey->AddActorLocalRotation(FRotator(0.f, Dt * 22.f, 0.f));
	}

	if (Entry.bDissolving)
	{
		Entry.Dissolve01 = FMath::Clamp(Entry.Dissolve01 + Dt / FMath::Max(0.2f, DissolveSeconds), 0.f, 1.f);
		const float Remain = 1.f - Entry.Dissolve01;
		const float S = FMath::Max(0.02f, Remain * Remain);
		Prey->SetActorRelativeScale3D(Entry.RestScale3D * S);
		if (UStaticMeshComponent* Mesh = Prey->MeshComponent)
		{
			Mesh->SetVisibility(Remain > 0.04f);
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
			{
				if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(I))
				{
					Mid->SetScalarParameterValue(TEXT("Opacity"), Remain);
					Mid->SetScalarParameterValue(TEXT("Opacity_Appearance"), Remain);
				}
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
		if (!Entries[I].Prey.IsValid()
			|| (Entries[I].bDissolving && Entries[I].Dissolve01 >= 1.f))
		{
			FinishEntry(Entries[I]);
			Entries.RemoveAtSwap(I);
		}
	}
}
