#include "SlimeVfx.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/Engine.h"
#include "RaisingSlimeTypes.h"

namespace
{
	const TCHAR* PathFor(ESlimeSplashKind Kind)
	{
		// Prefer Niagara Fluids FLIP splash; fall back to legacy RadialBurst copies.
		switch (Kind)
		{
		case ESlimeSplashKind::Spit:
			return TEXT("/Game/FX/NS_SpitSplash_Fluids.NS_SpitSplash_Fluids");
		case ESlimeSplashKind::Death:
			return TEXT("/Game/FX/NS_DeathSplash_Fluids.NS_DeathSplash_Fluids");
		default:
			return TEXT("/Game/FX/NS_DevourSplash_Fluids.NS_DevourSplash_Fluids");
		}
	}

	const TCHAR* LegacyPathFor(ESlimeSplashKind Kind)
	{
		switch (Kind)
		{
		case ESlimeSplashKind::Spit:
			return TEXT("/Game/FX/NS_SpitSplash.NS_SpitSplash");
		case ESlimeSplashKind::Death:
			return TEXT("/Game/FX/NS_DeathSplash.NS_DeathSplash");
		default:
			return TEXT("/Game/FX/NS_DevourSplash.NS_DevourSplash");
		}
	}

	const TCHAR* LabelFor(ESlimeSplashKind Kind)
	{
		switch (Kind)
		{
		case ESlimeSplashKind::Spit:
			return TEXT("Spit");
		case ESlimeSplashKind::Death:
			return TEXT("Death");
		default:
			return TEXT("Devour");
		}
	}

	const TCHAR* FootOozePath()
	{
		return TEXT("/Game/FX/NS_FootOoze_Fluids.NS_FootOoze_Fluids");
	}

	UNiagaraSystem* LoadSplash(ESlimeSplashKind Kind)
	{
		if (UNiagaraSystem* Fluids = LoadObject<UNiagaraSystem>(nullptr, PathFor(Kind)))
		{
			return Fluids;
		}
		return LoadObject<UNiagaraSystem>(nullptr, LegacyPathFor(Kind));
	}

	UNiagaraSystem* LoadFootOoze()
	{
		return LoadObject<UNiagaraSystem>(nullptr, FootOozePath());
	}

	void ScreenNote(UWorld* World, const FString& Msg, const FColor& Color)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3.5f, Color, Msg);
		}
		if (World)
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *Msg);
		}
	}
}

void SlimeVfx::Warmup(UWorld* World)
{
	const bool bDevour = LoadSplash(ESlimeSplashKind::Devour) != nullptr;
	const bool bSpit = LoadSplash(ESlimeSplashKind::Spit) != nullptr;
	const bool bDeath = LoadSplash(ESlimeSplashKind::Death) != nullptr;
	const bool bOoze = LoadFootOoze() != nullptr;
	const FString Msg = FString::Printf(
		TEXT("SlimeVfx: warmup devour=%d spit=%d death=%d footOoze=%d"),
		bDevour ? 1 : 0, bSpit ? 1 : 0, bDeath ? 1 : 0, bOoze ? 1 : 0);
	ScreenNote(World, Msg, (bDevour && bSpit && bDeath) ? FColor::Green : FColor::Orange);
}

void SlimeVfx::Play(UWorld* World, ESlimeSplashKind Kind, const FVector& Location, float BodySize)
{
	if (!World)
	{
		return;
	}

	// Devour must not read as a pop/burst — gut attach + float is the primary FX.
	if (Kind == ESlimeSplashKind::Devour)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SlimeVfx: devour splash suppressed (gut swallow)"));
		return;
	}

	UNiagaraSystem* System = LoadSplash(Kind);
	if (!System)
	{
		ScreenNote(
			World,
			FString::Printf(TEXT("SlimeVfx: MISSING %s (%s)"), LabelFor(Kind), PathFor(Kind)),
			FColor::Red);
		return;
	}

	// Fluids FLIP splash reads best slightly above ground; keep within ~2 m of body.
	const float Scale = FMath::Clamp(0.45f + BodySize * 0.35f, 0.45f, 1.35f);
	const FVector SpawnAt = Location + FVector(0.f, 0.f, 8.f);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		System,
		SpawnAt,
		FRotator::ZeroRotator,
		FVector(Scale),
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);

	ScreenNote(
		World,
		FString::Printf(TEXT("SlimeVfx: Fluids splash %s scale=%.2f"), LabelFor(Kind), Scale),
		FColor::Cyan);
}

void SlimeVfx::TickContactOoze(
	UWorld* World,
	AActor* Owner,
	float& InOutCooldown,
	const FVector& FeetLocation,
	float PlanarSpeedCm,
	bool bGrounded,
	float BodySize,
	float Dt)
{
	if (!World || !Owner)
	{
		return;
	}

	InOutCooldown = FMath::Max(0.f, InOutCooldown - Dt);

	// Viscous drip while sliding on contact — idle / air = silent.
	constexpr float MinSpeedCm = 28.f;
	if (!bGrounded || PlanarSpeedCm < MinSpeedCm)
	{
		return;
	}
	if (InOutCooldown > 0.f)
	{
		return;
	}

	UNiagaraSystem* System = LoadFootOoze();
	if (!System)
	{
		return;
	}

	// Faster crawl → slightly shorter gap; stay lazy (ooze, not spray).
	const float Speed01 = FMath::Clamp((PlanarSpeedCm - MinSpeedCm) / 420.f, 0.f, 1.f);
	InOutCooldown = FMath::Lerp(0.38f, 0.22f, Speed01);

	// Small scale — must not read as spit/death splash or devour pop.
	const float Scale = FMath::Clamp(0.12f + BodySize * 0.08f + Speed01 * 0.06f, 0.10f, 0.28f);
	const FVector SpawnAt = FeetLocation + FVector(0.f, 0.f, 2.f);

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		System,
		SpawnAt,
		FRotator::ZeroRotator,
		FVector(Scale),
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);

	UE_LOG(LogTemp, Verbose, TEXT("SlimeVfx: contact ooze scale=%.2f speed=%.0f"), Scale, PlanarSpeedCm);
}
