#include "SlimeGutContentComponent.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "RaisingSlimeTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

USlimeGutContentComponent::USlimeGutContentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

float USlimeGutContentComponent::GetPendingHeldMass() const
{
	float Total = 0.f;
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Held)
		{
			Total += E.Mass;
		}
	}
	return Total;
}

float USlimeGutContentComponent::GetContainedMass() const
{
	float Total = 0.f;
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Contained)
		{
			Total += E.Mass * E.Grow01;
		}
		else if (E.Phase == EGutPhase::Dissolving)
		{
			Total += E.Mass * E.Grow01 * FMath::Max(0.f, 1.f - E.Dissolve01);
		}
	}
	return Total;
}

float USlimeGutContentComponent::GetSwallowSwellBonus() const
{
	return GetContainedMass() * ContainedMassToSize;
}

ADevourable* USlimeGutContentComponent::FindPrimaryHeldPrey() const
{
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Held)
		{
			if (ADevourable* Prey = E.Prey.Get())
			{
				return Prey;
			}
		}
	}
	return nullptr;
}

void USlimeGutContentComponent::SyncClimbTarget()
{
	ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner());
	USlimeSoftVolumeComponent* Soft = SoftSlime ? SoftSlime->GetSoftVolume() : nullptr;
	if (!Soft)
	{
		return;
	}

	if (ADevourable* Prey = FindPrimaryHeldPrey())
	{
		Soft->SetEngulfClimbTarget(Prey);
	}
	else
	{
		Soft->ClearEngulfClimbTarget();
	}
}

void USlimeGutContentComponent::Swallow(ADevourable* Prey)
{
	if (!Prey || !GetOwner())
	{
		return;
	}

	Prey->BeginHeld();

	FGutEntry Entry;
	Entry.Prey = Prey;
	Entry.Phase = EGutPhase::Held;
	Entry.Mass = Prey->Mass;
	Entry.Trait = Prey->Trait;
	Entry.HeldWorldLocation = Prey->GetActorLocation();
	Entry.RestScale3D = Prey->GetActorScale3D();
	Entry.AbsorbRemaining = FMath::Max(0.5f, AbsorbDuration);
	Entry.PhaseBob = FMath::FRandRange(0.f, PI * 2.f);
	Entries.Add(Entry);

	SyncClimbTarget();

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Held %s — climb-wrap soft volume bottom→top (no center pull). gut=%d"),
		*Prey->GetLabel(), Entries.Num());
}

void USlimeGutContentComponent::EnterContained(FGutEntry& Entry)
{
	ADevourable* Prey = Entry.Prey.Get();
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Prey || !Root)
	{
		return;
	}

	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner()))
	{
		if (USlimeSoftVolumeComponent* Soft = SoftSlime->GetSoftVolume())
		{
			Soft->ClearEngulfClimbTarget();
		}
	}

	const FVector SealWorld = Prey->GetActorLocation();
	Entry.SealLocalOffset = Root->GetComponentTransform().InverseTransformPosition(SealWorld);
	Entry.HeldWorldLocation = SealWorld;
	Entry.Phase = EGutPhase::Contained;
	Entry.ContainedAge = 0.f;
	Entry.Grow01 = 0.f;
	Entry.AbsorbRemaining = FMath::Max(0.5f, AbsorbDuration);
	Entry.bAbsorbNotified = false;

	Prey->SealIntoGut(Root, Entry.SealLocalOffset);
	Entry.RestScale3D = Prey->GetActorRelativeScale3D();

	if (!Entry.bMassCommitted)
	{
		Entry.bMassCommitted = true;
		OnGutContained.ExecuteIfBound(Entry.Mass, Entry.Trait, Prey);
	}

	SyncClimbTarget();

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Contained %s after climb-wrap sealLocal=(%.1f,%.1f,%.1f)"),
		*Prey->GetLabel(), Entry.SealLocalOffset.X, Entry.SealLocalOffset.Y, Entry.SealLocalOffset.Z);
}

void USlimeGutContentComponent::TickHeldEngulf(FGutEntry& Entry, float Dt)
{
	ADevourable* Prey = Entry.Prey.Get();
	if (!Prey)
	{
		return;
	}

	Prey->SetActorLocation(Entry.HeldWorldLocation);
	Prey->SetActorHiddenInGame(false);

	SyncClimbTarget();

	float Climb01 = 0.f;
	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner()))
	{
		if (USlimeSoftVolumeComponent* Soft = SoftSlime->GetSoftVolume())
		{
			Climb01 = Soft->GetEngulfClimbProgress01();
		}
	}

	if (Climb01 >= EngulfClimbCompleteThreshold)
	{
		Entry.EngulfHoldAccum += Dt;
		if (Entry.EngulfHoldAccum >= EngulfHoldSeconds)
		{
			EnterContained(Entry);
		}
	}
	else
	{
		Entry.EngulfHoldAccum = FMath::Max(0.f, Entry.EngulfHoldAccum - Dt * 2.f);
	}
}

void USlimeGutContentComponent::BeginDissolveAll()
{
	for (FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Held)
		{
			E.bAbsorbNotified = true;
			continue;
		}
		E.Phase = EGutPhase::Dissolving;
		E.bAbsorbNotified = true;
	}
	SyncClimbTarget();
}

void USlimeGutContentComponent::ClearImmediate()
{
	for (FGutEntry& E : Entries)
	{
		FinishEntry(E);
	}
	Entries.Reset();
	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner()))
	{
		if (USlimeSoftVolumeComponent* Soft = SoftSlime->GetSoftVolume())
		{
			Soft->ClearEngulfClimbTarget();
		}
	}
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

	if (Entry.Phase == EGutPhase::Held)
	{
		TickHeldEngulf(Entry, Dt);
		return;
	}

	if (Entry.Phase == EGutPhase::Contained)
	{
		Entry.ContainedAge += Dt;
		if (Entry.ContainedAge <= GrowDelay)
		{
			Entry.Grow01 = 0.f;
		}
		else
		{
			const float T = (Entry.ContainedAge - GrowDelay) / FMath::Max(0.05f, GrowEaseDuration);
			const float Clamped = FMath::Clamp(T, 0.f, 1.f);
			Entry.Grow01 = 1.f - FMath::Pow(1.f - Clamped, 3.f);
		}

		if (!Entry.bAbsorbNotified)
		{
			Entry.AbsorbRemaining -= Dt;
			if (Entry.AbsorbRemaining <= 0.f)
			{
				Entry.AbsorbRemaining = 0.f;
				Entry.bAbsorbNotified = true;
				Entry.Phase = EGutPhase::Dissolving;
				OnGutAbsorbed.ExecuteIfBound(Entry.Mass, Entry.Trait, Prey);
				UE_LOG(LogTemp, Display, TEXT("SlimeGut: absorb complete mass=%.1f — dissolving at seal"),
					Entry.Mass);
			}
		}
	}

	Entry.PhaseBob += Dt * BobSpeed;
	const float Amp = FloatBobAmplitude;
	const FVector Bob(
		FMath::Sin(Entry.PhaseBob) * Amp * 0.35f,
		FMath::Cos(Entry.PhaseBob * 0.85f) * Amp * 0.35f,
		FMath::Sin(Entry.PhaseBob * 1.3f) * Amp);

	Prey->SetActorRelativeLocation(Entry.SealLocalOffset + Bob);
	Prey->AddActorLocalRotation(FRotator(0.f, Dt * 16.f, 0.f));

	if (Entry.Phase == EGutPhase::Dissolving)
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

	for (int32 I = Entries.Num() - 1; I >= 0; --I)
	{
		FGutEntry& E = Entries[I];
		if (E.Phase == EGutPhase::Held && E.bAbsorbNotified)
		{
			FinishEntry(E);
			Entries.RemoveAtSwap(I);
			continue;
		}

		TickEntry(E, DeltaTime);

		if (!E.Prey.IsValid()
			|| (E.Phase == EGutPhase::Dissolving && E.Dissolve01 >= 1.f))
		{
			FinishEntry(E);
			Entries.RemoveAtSwap(I);
		}
	}

	SyncClimbTarget();
}
