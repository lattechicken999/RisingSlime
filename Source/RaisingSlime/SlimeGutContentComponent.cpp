#include "SlimeGutContentComponent.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "SlimeMovementComponent.h"
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

float USlimeGutContentComponent::GetPrimarySink01() const
{
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Held)
		{
			return E.Sink01;
		}
	}
	return 0.f;
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

void USlimeGutContentComponent::GetPreyBounds(const ADevourable* Prey, FVector& OutCenter, float& OutRadiusXY, float& OutBottomZ, float& OutTopZ) const
{
	OutCenter = Prey ? Prey->GetActorLocation() : FVector::ZeroVector;
	OutRadiusXY = 28.f;
	OutBottomZ = OutCenter.Z - 25.f;
	OutTopZ = OutCenter.Z + 25.f;
	if (!Prey)
	{
		return;
	}
	if (Prey->MeshComponent)
	{
		const FBoxSphereBounds B = Prey->MeshComponent->Bounds;
		OutCenter = B.Origin;
		OutRadiusXY = FMath::Max(16.f, FMath::Max(B.BoxExtent.X, B.BoxExtent.Y));
		OutBottomZ = B.Origin.Z - B.BoxExtent.Z;
		OutTopZ = B.Origin.Z + B.BoxExtent.Z;
	}
	if (OutTopZ <= OutBottomZ + 1.f)
	{
		OutTopZ = OutBottomZ + 40.f;
	}
}

bool USlimeGutContentComponent::TryGetGlueSink(FVector& OutCenter, float& OutRadiusXY, float& OutBottomZ, float& OutTopZ, float& OutSink01) const
{
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase != EGutPhase::Held)
		{
			continue;
		}
		ADevourable* Prey = E.Prey.Get();
		if (!Prey || E.Sink01 < 0.02f)
		{
			continue;
		}
		GetPreyBounds(Prey, OutCenter, OutRadiusXY, OutBottomZ, OutTopZ);
		OutSink01 = E.Sink01;
		return true;
	}
	return false;
}

void USlimeGutContentComponent::ComputeGlueContact(const ADevourable* Prey, bool& bOutContact, float& OutRub01) const
{
	bOutContact = false;
	OutRub01 = 0.f;
	if (!Prey || !GetOwner())
	{
		return;
	}

	const ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner());
	FVector BodyXY = GetOwner()->GetActorLocation();
	float BodyR = 50.f;
	if (Pawn && Pawn->GetSoftVolume())
	{
		BodyXY = Pawn->GetSoftVolume()->GetBodyCenterWorld();
		BodyR = Pawn->GetSoftVolume()->GetBodyRadiusCm();
	}

	FVector PreyCenter;
	float PreyR, BottomZ, TopZ;
	GetPreyBounds(Prey, PreyCenter, PreyR, BottomZ, TopZ);

	const float DistXY = FVector::Dist2D(BodyXY, PreyCenter);
	const float ContactRange = BodyR + PreyR + 38.f;
	bOutContact = DistXY < ContactRange;
	if (!bOutContact)
	{
		return;
	}

	FVector ToPrey(PreyCenter.X - BodyXY.X, PreyCenter.Y - BodyXY.Y, 0.f);
	if (ToPrey.SizeSquared() < 1.f)
	{
		OutRub01 = 0.25f;
		return;
	}
	ToPrey.Normalize();
	const FVector Tangent(-ToPrey.Y, ToPrey.X, 0.f);

	FVector MoveXY = FVector::ZeroVector;
	if (Pawn && Pawn->GetSlimeMovement())
	{
		MoveXY = Pawn->GetSlimeMovement()->GetMoveIntent();
		MoveXY.Z = 0.f;
		if (MoveXY.SizeSquared() < 80.f)
		{
			MoveXY = Pawn->GetSlimeMovement()->GetPlanarVelocity();
			MoveXY.Z = 0.f;
		}
	}
	if (MoveXY.SizeSquared() < 4.f)
	{
		OutRub01 = 0.f;
		return;
	}
	MoveXY.Normalize();
	// Light rub = motion along prey tangent (or any planar move while stuck).
	OutRub01 = FMath::Clamp(FMath::Abs(FVector::DotProduct(MoveXY, Tangent)) + 0.35f * FMath::Abs(FVector::DotProduct(MoveXY, ToPrey)), 0.f, 1.f);
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
	Entry.Sink01 = 0.f;
	Entries.Add(Entry);

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

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Held %s — glue sink (world-pinned, no ribbons). gut=%d"),
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
		if (USlimeEngulfVenomComponent* Venom = SoftSlime->GetEngulfVenom())
		{
			Venom->Clear();
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

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Contained %s after Sink01=%.2f sealLocal=(%.1f,%.1f,%.1f)"),
		*Prey->GetLabel(), Entry.Sink01, Entry.SealLocalOffset.X, Entry.SealLocalOffset.Y, Entry.SealLocalOffset.Z);
}

void USlimeGutContentComponent::TickHeldGlue(FGutEntry& Entry, float Dt)
{
	ADevourable* Prey = Entry.Prey.Get();
	if (!Prey)
	{
		return;
	}

	// World-pinned — no COM yank.
	Prey->SetActorLocation(Entry.HeldWorldLocation);
	Prey->SetActorHiddenInGame(false);

	bool bContact = false;
	float Rub01 = 0.f;
	ComputeGlueContact(Prey, bContact, Rub01);

	if (bContact)
	{
		const float Rate = SinkHoldRate + Rub01 * SinkRubBonus;
		Entry.Sink01 = FMath::Clamp(Entry.Sink01 + Rate * Dt, 0.f, 1.f);
	}
	else
	{
		Entry.Sink01 = FMath::Max(0.f, Entry.Sink01 - SinkReleaseDecay * Dt);
	}

	static float GlueLogAccum = 0.f;
	GlueLogAccum += Dt;
	if (GlueLogAccum >= 0.35f)
	{
		GlueLogAccum = 0.f;
		UE_LOG(LogTemp, Display, TEXT("SlimeGut glue: Sink01=%.2f contact=%d rub=%.2f hold=%.2f"),
			Entry.Sink01, bContact ? 1 : 0, Rub01, Entry.SinkHoldAccum);
	}

	if (Entry.Sink01 >= SinkCompleteThreshold)
	{
		Entry.SinkHoldAccum += Dt;
		if (Entry.SinkHoldAccum >= SinkHoldSeconds)
		{
			EnterContained(Entry);
		}
	}
	else
	{
		Entry.SinkHoldAccum = FMath::Max(0.f, Entry.SinkHoldAccum - Dt * 2.f);
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
		TickHeldGlue(Entry, Dt);
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
}
