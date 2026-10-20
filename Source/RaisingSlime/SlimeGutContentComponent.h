#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaisingSlimeTypes.h"
#include "SlimeGutContentComponent.generated.h"

class ADevourable;

DECLARE_DELEGATE_ThreeParams(FOnGutAbsorbed, float /*Mass*/, ETraitId /*Trait*/, ADevourable* /*Prey*/);

/**
 * Gut visuals: swallowed prey attaches into the slime, floats while absorbing,
 * then dissolves. No burst VFX / instant despawn. Soft-volume physics unchanged.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeGutContentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeGutContentComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pull the real prey actor into the body and keep it visible while floating. */
	void Swallow(ADevourable* Prey);

	/** Start melting all gut contents (legacy settle / emergency). */
	void BeginDissolveAll();

	/** Death: hide gut prey immediately and queue respawn. */
	void ClearImmediate();

	int32 GetGutCount() const { return Entries.Num(); }

	/** Sustained size bonus from mass still floating inside (plus short swallow punch). */
	float GetSwallowSwellBonus() const;

	float GetContainedMass() const;

	FOnGutAbsorbed OnGutAbsorbed;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float AbsorbDuration = 8.f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float BobAmpCm = 10.f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float BobSpeed = 1.6f;

	/** Scale prey inside gut relative to its world scale (readable, not tiny). */
	UPROPERTY(EditAnywhere, Category = "Gut")
	float FitScale = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float DissolveSeconds = 2.4f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float PullInSeconds = 0.55f;

	/** Extra body-size punch on swallow so growth reads immediately. */
	UPROPERTY(EditAnywhere, Category = "Gut")
	float SwallowSwell = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float SwellDecayPerSec = 0.35f;

	/** Body-size contribution per unit of mass still inside the gut. */
	UPROPERTY(EditAnywhere, Category = "Gut")
	float ContainedMassToSize = 0.07f;

protected:
	struct FGutEntry
	{
		TWeakObjectPtr<ADevourable> Prey;
		float Mass = 0.f;
		ETraitId Trait = ETraitId::None;
		float AbsorbRemaining = 8.f;
		bool bAbsorbNotified = false;
		FVector BaseLocal = FVector::ZeroVector;
		FVector PullStartLocal = FVector::ZeroVector;
		FVector RestScale3D = FVector::OneVector;
		float Phase = 0.f;
		float Pull01 = 0.f;
		float Dissolve01 = 0.f;
		bool bPulling = true;
		bool bDissolving = false;
	};

	void TickEntry(FGutEntry& Entry, float Dt);
	void FinishEntry(FGutEntry& Entry);
	FVector GutAnchorLocal() const;

	TArray<FGutEntry> Entries;
	float SwellBonus = 0.f;
};
