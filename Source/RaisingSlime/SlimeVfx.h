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
	 * Item-gated foot slime trail: short-lived footprint mark where the slime passed.
	 * Caller must gate (ASlimePawn::bFootOozeFromItem) — default movement must NOT call this.
	 * NOT Niagara Fluids / SW_Drop — no sideways water spread or persistent pool.
	 */
	void TickFootSlimeTrail(
		UWorld* World,
		AActor* Owner,
		float& InOutCooldown,
		const FVector& FeetLocation,
		float PlanarSpeedCm,
		bool bGrounded,
		float BodySize,
		float Dt);

	/** @deprecated Name kept for call-site clarity during rename — forwards to TickFootSlimeTrail. */
	inline void TickContactOoze(
		UWorld* World,
		AActor* Owner,
		float& InOutCooldown,
		const FVector& FeetLocation,
		float PlanarSpeedCm,
		bool bGrounded,
		float BodySize,
		float Dt)
	{
		TickFootSlimeTrail(World, Owner, InOutCooldown, FeetLocation, PlanarSpeedCm, bGrounded, BodySize, Dt);
	}
}
