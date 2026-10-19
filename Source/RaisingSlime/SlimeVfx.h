#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UWorld;

/** Spec residual: splash FX within ~2 m of the body (Niagara, not Fluids yet). */
enum class ESlimeSplashKind : uint8
{
	Devour,
	Spit,
	Death
};

namespace SlimeVfx
{
	/** Soft-load assets once; uses Display verbosity so Output Log shows it by default. */
	void Warmup(UWorld* World);
	void Play(UWorld* World, ESlimeSplashKind Kind, const FVector& Location, float BodySize = 1.f);
}
