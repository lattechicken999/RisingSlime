#include "SlimeGutContentComponent.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeSoftVolumeComponent.h"
#include "SlimeMovementComponent.h"
#include "SlimeEngulfVenomComponent.h"
#include "RaisingSlimeTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

USlimeGutContentComponent::USlimeGutContentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bVenomDemo = FParse::Param(FCommandLine::Get(), TEXT("VenomEngulfDemo"));
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

float USlimeGutContentComponent::GetPrimaryEngulf01() const
{
	for (const FGutEntry& E : Entries)
	{
		if (E.Phase == EGutPhase::Held)
		{
			return E.Engulf01;
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

USlimeEngulfVenomComponent* USlimeGutContentComponent::GetVenom() const
{
	ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner());
	return Pawn ? Pawn->GetEngulfVenom() : nullptr;
}

void USlimeGutContentComponent::SyncVenomPresentation()
{
	USlimeEngulfVenomComponent* Venom = GetVenom();
	if (!Venom)
	{
		return;
	}

	ADevourable* Prey = FindPrimaryHeldPrey();
	if (!Prey)
	{
		return;
	}

	if (!Venom->IsActive())
	{
		UMaterialInterface* Mat = nullptr;
		if (ASlimePawn* Pawn = Cast<ASlimePawn>(GetOwner()))
		{
			Mat = Pawn->GetCachedSlimeMaterial();
		}
		if (!Mat)
		{
			Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Slime.M_Slime"));
		}
		Venom->BeginEngulf(Prey, Mat);
	}
	Venom->SetEngulfProgress01(GetPrimaryEngulf01());
}

void USlimeGutContentComponent::ComputePressContact(const ADevourable* Prey, float& OutPress, bool& bOutContact) const
{
	OutPress = 0.f;
	bOutContact = false;
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

	FVector PreyOrigin = Prey->GetActorLocation();
	float PreyR = 30.f;
	if (Prey->MeshComponent)
	{
		const FBoxSphereBounds B = Prey->MeshComponent->Bounds;
		PreyOrigin = B.Origin;
		PreyR = FMath::Max(16.f, FMath::Max(B.BoxExtent.X, B.BoxExtent.Y));
	}

	const float DistXY = FVector::Dist2D(BodyXY, PreyOrigin);
	const float ContactRange = BodyR + PreyR + 42.f;
	bOutContact = DistXY < ContactRange;

	FVector ToPrey(PreyOrigin.X - BodyXY.X, PreyOrigin.Y - BodyXY.Y, 0.f);
	if (ToPrey.SizeSquared() < 1.f)
	{
		OutPress = bOutContact ? 0.35f : 0.f;
		return;
	}
	ToPrey.Normalize();

	FVector MoveXY = FVector::ZeroVector;
	if (Pawn && Pawn->GetSlimeMovement())
	{
		MoveXY = Pawn->GetSlimeMovement()->GetMoveIntent();
		MoveXY.Z = 0.f;
		if (MoveXY.SizeSquared() < 100.f)
		{
			MoveXY = Pawn->GetSlimeMovement()->GetPlanarVelocity();
			MoveXY.Z = 0.f;
		}
	}

	if (MoveXY.SizeSquared() < 4.f)
	{
		OutPress = 0.f;
		return;
	}
	MoveXY.Normalize();
	OutPress = FMath::Clamp(FVector::DotProduct(MoveXY, ToPrey), 0.f, 1.f);
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
	Entry.Engulf01 = 0.f;
	Entries.Add(Entry);

	// Ensure climb sleeve stays off.
	if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwner()))
	{
		if (USlimeSoftVolumeComponent* Soft = SoftSlime->GetSoftVolume())
		{
			Soft->ClearEngulfClimbTarget();
		}
	}

	SyncVenomPresentation();

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Held %s — press-meter + venom wrap in place (no climb sleeve). gut=%d"),
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

	if (USlimeEngulfVenomComponent* Venom = GetVenom())
	{
		Venom->BeginFadeOut();
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

	UE_LOG(LogTemp, Display, TEXT("SlimeGut: Contained %s after Engulf01=%.2f sealLocal=(%.1f,%.1f,%.1f)"),
		*Prey->GetLabel(), Entry.Engulf01, Entry.SealLocalOffset.X, Entry.SealLocalOffset.Y, Entry.SealLocalOffset.Z);
}

void USlimeGutContentComponent::TickHeldEngulf(FGutEntry& Entry, float Dt)
{
	ADevourable* Prey = Entry.Prey.Get();
	if (!Prey)
	{
		return;
	}

	// Stay in place — no COM yank.
	Prey->SetActorLocation(Entry.HeldWorldLocation);
	Prey->SetActorHiddenInGame(false);

	float Press = 0.f;
	bool bContact = false;
	ComputePressContact(Prey, Press, bContact);
	if (bVenomDemo)
	{
		// Demo: hold contact + steady press so ribbons climb without manual WASD.
		bContact = true;
		Press = 1.f;
	}

	if (bContact)
	{
		const float Delta = Press * EngulfChargeRate - EngulfIdleDecay;
		Entry.Engulf01 = FMath::Clamp(Entry.Engulf01 + Delta * Dt, 0.f, 1.f);
	}
	else
	{
		Entry.Engulf01 = FMath::Max(0.f, Entry.Engulf01 - EngulfReleaseDecay * Dt);
	}

	SyncVenomPresentation();

	static float EngulfLogAccum = 0.f;
	EngulfLogAccum += Dt;
	if (EngulfLogAccum >= 0.35f)
	{
		EngulfLogAccum = 0.f;
		UE_LOG(LogTemp, Display, TEXT("SlimeGut engulf: Engulf01=%.2f press=%.2f contact=%d hold=%.2f"),
			Entry.Engulf01, Press, bContact ? 1 : 0, Entry.EngulfHoldAccum);
	}

	if (Entry.Engulf01 >= EngulfCompleteThreshold)
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
	if (USlimeEngulfVenomComponent* Venom = GetVenom())
	{
		Venom->Clear();
	}
}

void USlimeGutContentComponent::ClearImmediate()
{
	for (FGutEntry& E : Entries)
	{
		FinishEntry(E);
	}
	Entries.Reset();
	if (USlimeEngulfVenomComponent* Venom = GetVenom())
	{
		Venom->Clear();
	}
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

void USlimeGutContentComponent::TickVenomDemo(float Dt)
{
	if (!bVenomDemo || !GetWorld() || !GetOwner())
	{
		return;
	}

	VenomDemoAge += Dt;

	// Phase 0: after load, find Prey_Rock / 돌, park beside it, swallow.
	if (VenomDemoAge > 1.2f && Entries.Num() == 0)
	{
		TArray<AActor*> PreyActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADevourable::StaticClass(), PreyActors);
		ADevourable* Best = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		const FVector Here = GetOwner()->GetActorLocation();
		for (AActor* A : PreyActors)
		{
			ADevourable* D = Cast<ADevourable>(A);
			if (!D || !D->CanDevour())
			{
				continue;
			}
			const FString Label = D->GetLabel();
			const bool bRock = Label.Contains(TEXT("돌")) || Label.Contains(TEXT("Rock"), ESearchCase::IgnoreCase)
				|| D->GetName().Contains(TEXT("Rock"), ESearchCase::IgnoreCase);
			const float Dist = FVector::Dist(Here, D->GetActorLocation());
			if (bRock && Dist < BestDist)
			{
				BestDist = Dist;
				Best = D;
			}
		}
		if (!Best && PreyActors.Num() > 0)
		{
			Best = Cast<ADevourable>(PreyActors[0]);
		}
		if (Best)
		{
			FVector PreyLoc = Best->GetActorLocation();
			FVector Beside = PreyLoc + FVector(55.f, 0.f, 0.f);
			Beside.Z = GetOwner()->GetActorLocation().Z;
			GetOwner()->SetActorLocation(Beside);
			Swallow(Best);
			UE_LOG(LogTemp, Display, TEXT("VenomEngulfDemo: swallowed %s for wrap capture"), *Best->GetLabel());
		}
	}

	// HighResShot at wrap stages (writes Saved/Screenshots).
	const float Engulf = GetPrimaryEngulf01();
	auto Shot = [&](const TCHAR* Tag)
	{
		if (GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("HighResShot 1920x1080"));
			UE_LOG(LogTemp, Display, TEXT("VenomEngulfDemo: HighResShot %s Engulf01=%.2f"), Tag, Engulf);
		}
	};

	if (VenomDemoShot == 0 && Engulf >= 0.25f)
	{
		Shot(TEXT("wrap25"));
		VenomDemoShot = 1;
	}
	else if (VenomDemoShot == 1 && Engulf >= 0.55f)
	{
		Shot(TEXT("wrap55"));
		VenomDemoShot = 2;
	}
	else if (VenomDemoShot == 2 && Engulf >= 0.9f)
	{
		Shot(TEXT("wrap90"));
		VenomDemoShot = 3;
	}
	else if (VenomDemoShot == 3)
	{
		bool bContainedShot = false;
		for (const FGutEntry& E : Entries)
		{
			if (E.Phase == EGutPhase::Contained && E.ContainedAge > 0.25f)
			{
				bContainedShot = true;
				break;
			}
		}
		if (bContainedShot)
		{
			Shot(TEXT("contained"));
			VenomDemoShot = 4;
			VenomDemoQuitAccum = 0.f;
			UE_LOG(LogTemp, Display, TEXT("VenomEngulfDemo: DONE — quit in 1.5s"));
		}
	}
	else if (VenomDemoShot == 4)
	{
		// Do NOT call quit from gameplay — that looked like a crash during predation.
		// External harness kills the process after shots land in Saved/Screenshots.
		VenomDemoQuitAccum += Dt;
		if (VenomDemoQuitAccum > 0.5f && VenomDemoShot == 4)
		{
			UE_LOG(LogTemp, Display, TEXT("VenomEngulfDemo: capture complete — idle (no RequestExit)"));
			VenomDemoShot = 5;
		}
	}
}

void USlimeGutContentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickVenomDemo(DeltaTime);

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

	if (!FindPrimaryHeldPrey())
	{
		// Held cleared — venom should already be fading from Contained; if cancelled, clear.
		if (USlimeEngulfVenomComponent* Venom = GetVenom())
		{
			if (Venom->IsActive() && Entries.Num() == 0)
			{
				Venom->Clear();
			}
		}
	}
}
