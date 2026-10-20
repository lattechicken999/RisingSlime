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
 * Natural devour: Held + ferrofluid climb-wrap → Contained float at seal → dissolve.
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

	FOnGutAbsorbed OnGutAbsorbed;
	FOnGutContained OnGutContained;

	UPROPERTY(EditAnywhere, Category = "Gut|Absorb")
	float AbsorbDuration = 8.f;

	/** Contained when soft-volume climb front reaches this (bottom→top). */
	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float EngulfClimbCompleteThreshold = 0.92f;

	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float EngulfHoldSeconds = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Gut|Float")
	float FloatBobAmplitude = 6.f;

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
		FVector HeldWorldLocation = FVector::ZeroVector;
		FVector SealLocalOffset = FVector::ZeroVector;
		FVector RestScale3D = FVector::OneVector;
		float EngulfHoldAccum = 0.f;
		float ContainedAge = 0.f;
		float Grow01 = 0.f;
		float AbsorbRemaining = 8.f;
		bool bAbsorbNotified = false;
		bool bMassCommitted = false;
		float PhaseBob = 0.f;
		float Dissolve01 = 0.f;
	};

	void TickEntry(FGutEntry& Entry, float Dt);
	void TickHeldEngulf(FGutEntry& Entry, float Dt);
	void EnterContained(FGutEntry& Entry);
	void FinishEntry(FGutEntry& Entry);
	void SyncClimbTarget();
	ADevourable* FindPrimaryHeldPrey() const;

	TArray<FGutEntry> Entries;
};
