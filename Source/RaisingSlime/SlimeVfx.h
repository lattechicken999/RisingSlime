#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UWorld;
class AActor;

/** Spec residual: splash FX within ~2 m of the body (Niagara Fluids VFX only — not gameplay physics). */
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

	/**
	 * Subtle viscous foot/contact ooze while grounded + moving.
	 * NOT splashy, NOT devour-pop. Rate-limited Fluids drop (or silent if asset missing).
	 */
	void TickContactOoze(
		UWorld* World,
		AActor* Owner,
		float& InOutCooldown,
		const FVector& FeetLocation,
		float PlanarSpeedCm,
		bool bGrounded,
		float BodySize,
		float Dt);
}
