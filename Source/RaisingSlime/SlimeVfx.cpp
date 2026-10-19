#include "SlimeVfx.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RaisingSlimeTypes.h"

namespace
{
	UNiagaraSystem* LoadSplash(ESlimeSplashKind Kind)
	{
		const TCHAR* Path = TEXT("/Game/FX/NS_DevourSplash.NS_DevourSplash");
		float DefaultScale = 1.f;
		switch (Kind)
		{
		case ESlimeSplashKind::Spit:
			Path = TEXT("/Game/FX/NS_SpitSplash.NS_SpitSplash");
			break;
		case ESlimeSplashKind::Death:
			Path = TEXT("/Game/FX/NS_DeathSplash.NS_DeathSplash");
			break;
		default:
			break;
		}
		(void)DefaultScale;
		return LoadObject<UNiagaraSystem>(nullptr, Path);
	}
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
		UE_LOG(LogTemp, Warning, TEXT("SlimeVfx: missing Niagara asset for kind %d (run Content/Python/create_slime_vfx.py)"),
			static_cast<int32>(Kind));
		return;
	}

	// Spec: sim domain stays within ~2 m of the body.
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

	UE_LOG(LogTemp, Log, TEXT("SlimeVfx: spawned kind %d scale=%.2f"), static_cast<int32>(Kind), Scale);
}
