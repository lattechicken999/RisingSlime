#include "SlimeVfx.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/Engine.h"
#include "RaisingSlimeTypes.h"

namespace
{
	const TCHAR* PathFor(ESlimeSplashKind Kind)
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
		return LoadObject<UNiagaraSystem>(nullptr, PathFor(Kind));
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
	const FString Msg = FString::Printf(
		TEXT("SlimeVfx: warmup devour=%d spit=%d death=%d (삼키면/토하면/죽으면 다시 로그)"),
		bDevour ? 1 : 0, bSpit ? 1 : 0, bDeath ? 1 : 0);
	ScreenNote(World, Msg, (bDevour && bSpit && bDeath) ? FColor::Green : FColor::Orange);
}

void SlimeVfx::Play(UWorld* World, ESlimeSplashKind Kind, const FVector& Location, float BodySize)
{
	if (!World)
	{
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

	const float Scale = FMath::Clamp(0.35f + BodySize * 0.25f, 0.35f, 1.1f);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		System,
		Location,
		FRotator::ZeroRotator,
		FVector(Scale),
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);

	ScreenNote(
		World,
		FString::Printf(TEXT("SlimeVfx: spawned %s scale=%.2f"), LabelFor(Kind), Scale),
		FColor::Cyan);
}
