#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaisingSlimeTypes.h"
#include "SlimeGutContentComponent.generated.h"

class ADevourable;

UENUM()
enum class EGutPhase : uint8
{
	Held,
	Contained,
	Dissolving
};

DECLARE_DELEGATE_ThreeParams(FOnGutAbsorbed, float /*Mass*/, ETraitId /*Trait*/, ADevourable* /*Prey*/);
DECLARE_DELEGATE_ThreeParams(FOnGutContained, float /*Mass*/, ETraitId /*Trait*/, ADevourable* /*Prey*/);

/**
 * Surface drift devour: stick to outer surface → overdamped drift to center → Contained.
 * No world-pin, no COM yank, no venom/climb Contained authority.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeGutContentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeGutContentComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Swallow(ADevourable* Prey);
	void BeginDissolveAll();
	void ClearImmediate();

	int32 GetGutCount() const { return Entries.Num(); }
	float GetSwallowSwellBonus() const;
	float GetContainedMass() const;
	float GetPendingHeldMass() const;

	/** 0 = on surface, 1 = at Contained radius (for debug / optional skin). */
	float GetPrimaryDrift01() const;

	FOnGutAbsorbed OnGutAbsorbed;
	FOnGutContained OnGutContained;

	UPROPERTY(EditAnywhere, Category = "Gut|Absorb")
	float AbsorbDuration = 8.f;

	/** Stick distance as fraction of body radius. */
	UPROPERTY(EditAnywhere, Category = "Gut|Drift", meta = (ClampMin = "0.7", ClampMax = "1.2"))
	float SurfaceStickScale = 0.95f;

	/**
	 * Overdamped radial drift rate (1/s). ~0.85 → surface→Contained (~0.35R) in ~1.0–1.8s.
	 */
	UPROPERTY(EditAnywhere, Category = "Gut|Drift", meta = (ClampMin = "0.2", ClampMax = "3"))
	float DriftRate = 0.85f;

	/** Contained when |offset - center| / BodyRadius <= this. */
	UPROPERTY(EditAnywhere, Category = "Gut|Drift", meta = (ClampMin = "0.15", ClampMax = "0.55"))
	float ContainedRadiusFraction = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Gut|Drift", meta = (ClampMin = "0.05", ClampMax = "0.4"))
	float ContainedHoldSeconds = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Gut|Float")
	float FloatBobAmplitude = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Gut|Float")
	float BobSpeed = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Gut|Grow", meta = (ClampMin = "0"))
	float GrowDelay = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Gut|Grow", meta = (ClampMin = "0.1"))
	float GrowEaseDuration = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Gut|Grow")
	float ContainedMassToSize = 0.07f;

	UPROPERTY(EditAnywhere, Category = "Gut|Absorb")
	float DissolveSeconds = 2.4f;

protected:
	struct FGutEntry
	{
		TWeakObjectPtr<ADevourable> Prey;
		EGutPhase Phase = EGutPhase::Held;
		float Mass = 0.f;
		ETraitId Trait = ETraitId::None;
		FVector StickLocalOffset = FVector::ZeroVector;
		FVector SealLocalOffset = FVector::ZeroVector;
		FVector RestScale3D = FVector::OneVector;
		float ContainedHoldAccum = 0.f;
		float ContainedAge = 0.f;
		float Grow01 = 0.f;
		float AbsorbRemaining = 8.f;
		bool bAbsorbNotified = false;
		bool bMassCommitted = false;
		float PhaseBob = 0.f;
		float Dissolve01 = 0.f;
	};

	void TickEntry(FGutEntry& Entry, float Dt);
	void TickHeldDrift(FGutEntry& Entry, float Dt);
	void EnterContained(FGutEntry& Entry);
	void FinishEntry(FGutEntry& Entry);
	ADevourable* FindPrimaryHeldPrey() const;
	float GetBodyRadiusCm() const;
	FVector GetBodyCenterLocal(const USceneComponent* Root) const;
	void AttachPreyToSurface(ADevourable* Prey, FGutEntry& Entry);

	TArray<FGutEntry> Entries;
};
