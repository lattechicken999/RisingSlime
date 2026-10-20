#include "SlimeGutContentComponent.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "SlimeEngulfVenomComponent.h"
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

float USlimeGutContentComponent::GetBodyRadiusCm() const
{
	if (const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner()))
	{
		if (const USlimeSoftVolumeComponent* Soft = Pawn->GetSoftVolume())
		{
			return Soft->GetBodyRadiusCm();
		}
		return Pawn->GetBodySize() * 50.f;
	}
	return 50.f;
}

FVector USlimeGutContentComponent::GetBodyCenterLocal(const USceneComponent* Root) const
{
	if (!Root)
	{
		return FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm);
	}
	FVector CenterW = Root->GetComponentLocation() + FVector(0.f, 0.f, RaisingSlimeUnits::BodyHeightCm);
	if (const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner()))
	{
		if (const USlimeSoftVolumeComponent* Soft = Pawn->GetSoftVolume())
		{
			CenterW = Soft->GetBodyCenterWorld();
		}
	}
	return Root->GetComponentTransform().InverseTransformPosition(CenterW);
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

float USlimeGutContentComponent::GetPrimaryDrift01() const
{
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Root)
	{
		return 0.f;
	}
	const FVector CenterLocal = GetBodyCenterLocal(Root);
	const float BodyR = FMath::Max(20.f, GetBodyRadiusCm());
	const float SurfaceDist = BodyR * SurfaceStickScale;
	const float ContainedDist = BodyR * ContainedRadiusFraction;
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase != EGutPhase::Held)
		{
			continue;
		}
		const float Dist = FVector::Dist(E.StickLocalOffset, CenterLocal);
		const float Span = FMath::Max(1.f, SurfaceDist - ContainedDist);
		return FMath::Clamp(1.f - (Dist - ContainedDist) / Span, 0.f, 1.f);
	}
	return 0.f;
}

void USlimeGutContentComponent::AttachPreyToSurface(ADevourable* Prey, FGutEntry& Entry)
{
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Prey || !Root)
	{
		return;
	}

	const FVector CenterLocal = GetBodyCenterLocal(Root);
	const float BodyR = FMath::Max(20.f, GetBodyRadiusCm());
	FVector PreyLocal = Root->GetComponentTransform().InverseTransformPosition(Prey->GetActorLocation());
	FVector Dir = PreyLocal - CenterLocal;
	Dir.Z *= 0.65f; // prefer side/front stick on the blob, not underground
	if (Dir.SizeSquared() < 1.f)
	{
		Dir = FVector(1.f, 0.f, 0.15f);
	}
	Dir.Normalize();
	Entry.StickLocalOffset = CenterLocal + Dir * (BodyR * SurfaceStickScale);

	const FVector WorldScale = Prey->GetActorScale3D();
	Prey->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Prey->SetActorRelativeLocation(Entry.StickLocalOffset);
	Prey->SetActorRelativeScale3D(WorldScale);
	Prey->SetActorHiddenInGame(false);
	Entry.RestScale3D = Prey->GetActorRelativeScale3D();
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
	Entry.AbsorbRemaining = FMath::Max(0.5f, AbsorbDuration);
	Entry.PhaseBob = FMath::FRandRange(0.f, PI * 2.f);
	Entries.Add(Entry);
	FGutEntry& Added = Entries.Last();
	AttachPreyToSurface(Prey, Added);

	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner()))
	{
		if (USlimeSoftVolumeComponent* Soft = SoftSlime->GetSoftVolume())
		{
			Soft->ClearEngulfClimbTarget();
		}
		if (USlimeEngulfVenomComponent* Venom = SoftSlime->GetEngulfVenom())
		{
			Venom->Clear();
			Venom->SetComponentTickEnabled(false);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Held %s — surface stick then drift to center. gut=%d stickLocal=(%.1f,%.1f,%.1f)"),
		*Prey->GetLabel(), Entries.Num(),
		Added.StickLocalOffset.X, Added.StickLocalOffset.Y, Added.StickLocalOffset.Z);
}

void USlimeGutContentComponent::EnterContained(FGutEntry& Entry)
{
	ADevourable* Prey = Entry.Prey.Get();
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Prey || !Root)
	{
		return;
	}

	Entry.SealLocalOffset = Entry.StickLocalOffset;
	Entry.Phase = EGutPhase::Contained;
	Entry.ContainedAge = 0.f;
	Entry.Grow01 = 0.f;
	Entry.AbsorbRemaining = FMath::Max(0.5f, AbsorbDuration);
	Entry.bAbsorbNotified = false;

	// Already attached from Held — refresh seal pose.
	Prey->SealIntoGut(Root, Entry.SealLocalOffset);
	Entry.RestScale3D = Prey->GetActorRelativeScale3D();

	if (!Entry.bMassCommitted)
	{
		Entry.bMassCommitted = true;
		OnGutContained.ExecuteIfBound(Entry.Mass, Entry.Trait, Prey);
	}

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Contained %s after surface-drift sealLocal=(%.1f,%.1f,%.1f)"),
		*Prey->GetLabel(), Entry.SealLocalOffset.X, Entry.SealLocalOffset.Y, Entry.SealLocalOffset.Z);
}

void USlimeGutContentComponent::TickHeldDrift(FGutEntry& Entry, float Dt)
{
	ADevourable* Prey = Entry.Prey.Get();
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Prey || !Root)
	{
		return;
	}

	const FVector CenterLocal = GetBodyCenterLocal(Root);
	const float BodyR = FMath::Max(20.f, GetBodyRadiusCm());
	const float ContainedDist = BodyR * ContainedRadiusFraction;

	// Overdamped radial creep toward body center (readable, not instant yank).
	const float Alpha = 1.f - FMath::Exp(-FMath::Max(0.15f, DriftRate) * Dt);
	Entry.StickLocalOffset = FMath::Lerp(Entry.StickLocalOffset, CenterLocal, Alpha);

	Prey->SetActorRelativeLocation(Entry.StickLocalOffset);
	Prey->SetActorHiddenInGame(false);

	const float Dist = FVector::Dist(Entry.StickLocalOffset, CenterLocal);
	const float Drift01 = GetPrimaryDrift01();

	static float DriftLogAccum = 0.f;
	DriftLogAccum += Dt;
	if (DriftLogAccum >= 0.35f)
	{
		DriftLogAccum = 0.f;
		UE_LOG(LogTemp, Display, TEXT("SlimeGut drift: dist=%.1f / R=%.1f drift01=%.2f hold=%.2f"),
			Dist, BodyR, Drift01, Entry.ContainedHoldAccum);
	}

	if (Dist <= ContainedDist)
	{
		Entry.ContainedHoldAccum += Dt;
		if (Entry.ContainedHoldAccum >= ContainedHoldSeconds)
		{
			EnterContained(Entry);
		}
	}
	else
	{
		Entry.ContainedHoldAccum = FMath::Max(0.f, Entry.ContainedHoldAccum - Dt * 2.f);
	}
}

bool USlimeGutContentComponent::HasSpittableGutPrey() const
{
	for (const FGutEntry& E : Entries)
	{
		if ((E.Phase == EGutPhase::Held || E.Phase == EGutPhase::Contained) && E.Prey.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool USlimeGutContentComponent::TrySpitUnabsorbedPrey(
	const FVector& LaunchOrigin,
	const FVector& LaunchVelocity,
	float& OutMass,
	ETraitId& OutTrait,
	bool& bOutMassCommitted,
	FString& OutLabel)
{
	OutMass = 0.f;
	OutTrait = ETraitId::None;
	bOutMassCommitted = false;
	OutLabel.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	for (int32 I = Entries.Num() - 1; I >= 0; --I)
	{
		FGutEntry& E = Entries[I];
		if (E.Phase != EGutPhase::Held && E.Phase != EGutPhase::Contained)
		{
			continue;
		}
		ADevourable* Prey = E.Prey.Get();
		if (!Prey)
		{
			Entries.RemoveAt(I);
			continue;
		}

		OutMass = E.Mass;
		OutTrait = E.Trait;
		bOutMassCommitted = E.bMassCommitted;
		OutLabel = Prey->GetLabel();
		const bool bWasHeld = (E.Phase == EGutPhase::Held);

		Prey->ReleaseFromGutToWorld(LaunchOrigin, LaunchVelocity);

		Entries.RemoveAt(I);

		if (ASlimePawn* Slime = Cast<ASlimePawn>(Owner))
		{
			if (USlimeSoftVolumeComponent* Soft = Slime->GetSoftVolume())
			{
				Soft->ClearEngulfClimbTarget();
			}
		}

		UE_LOG(LogTemp, Display,
			TEXT("SlimeGut: launched %s (phase=%s massCommitted=%d speed=%.0f)"),
			*OutLabel,
			bWasHeld ? TEXT("Held") : TEXT("Contained"),
			bOutMassCommitted ? 1 : 0,
			LaunchVelocity.Size());
		return true;
	}
	return false;
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
		if (USlimeEngulfVenomComponent* Venom = SoftSlime->GetEngulfVenom())
		{
			Venom->Clear();
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
		TickHeldDrift(Entry, Dt);
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
				UE_LOG(LogTemp, Display, TEXT("SlimeGut: absorb complete mass=%.1f — dissolving inside"),
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
}
