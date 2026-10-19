#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

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
	void Play(UWorld* World, ESlimeSplashKind Kind, const FVector& Location, float BodySize = 1.f);
}
