#include "SlimeVfx.h"
#include "SlimeTrailMark.h"
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

	UNiagaraSystem* LoadSplash(ESlimeSplashKind Kind)
	{
		if (UNiagaraSystem* Fluids = LoadObject<UNiagaraSystem>(nullptr, PathFor(Kind)))
		{
			return Fluids;
		}
		return LoadObject<UNiagaraSystem>(nullptr, LegacyPathFor(Kind));
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
	// Foot trail is C++ ASlimeTrailMark — NS_FootOoze_Fluids / SW_Drop intentionally unused.
	const FString Msg = FString::Printf(
		TEXT("SlimeVfx: warmup devour=%d spit=%d death=%d footTrail=cpp(no SW_Drop)"),
		bDevour ? 1 : 0, bSpit ? 1 : 0, bDeath ? 1 : 0);
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

void SlimeVfx::TickFootSlimeTrail(
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

	// Trail only while sliding on contact — idle / air = silent.
	constexpr float MinSpeedCm = 28.f;
	if (!bGrounded || PlanarSpeedCm < MinSpeedCm)
	{
		return;
	}
	if (InOutCooldown > 0.f)
	{
		return;
	}

	const float Speed01 = FMath::Clamp((PlanarSpeedCm - MinSpeedCm) / 420.f, 0.f, 1.f);
	// Sparse marks along the path (not a continuous water sheet).
	InOutCooldown = FMath::Lerp(0.42f, 0.26f, Speed01);

	// Footprint-sized: slightly larger than contact only.
	const float DiameterCm = FMath::Clamp(28.f + BodySize * 22.f + Speed01 * 8.f, 24.f, 72.f);
	const float Lifetime = FMath::Lerp(0.95f, 0.65f, Speed01);

	ASlimeTrailMark::SpawnAt(World, FeetLocation, DiameterCm, Lifetime);

	UE_LOG(LogTemp, Verbose, TEXT("SlimeVfx: foot trail diam=%.0fcm life=%.2f speed=%.0f"),
		DiameterCm, Lifetime, PlanarSpeedCm);
}
