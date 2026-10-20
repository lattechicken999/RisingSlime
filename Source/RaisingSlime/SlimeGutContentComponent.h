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
 * Glue sink (option C): Held world-pinned → Sink01 while stuck/rubbing → Contained seal float.
 * No COM pull, no venom ribbons, no climb sleeve Contained authority.
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
	float GetPrimarySink01() const;

	/** Held glue target for local jelly deform (world-pinned prey). */
	bool TryGetGlueSink(FVector& OutCenter, float& OutRadiusXY, float& OutBottomZ, float& OutTopZ, float& OutSink01) const;

	FOnGutAbsorbed OnGutAbsorbed;
	FOnGutContained OnGutContained;

	UPROPERTY(EditAnywhere, Category = "Gut|Absorb")
	float AbsorbDuration = 8.f;

	/** Contained when Sink01 reaches this. */
	UPROPERTY(EditAnywhere, Category = "Gut|Glue", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float SinkCompleteThreshold = 0.98f;

	UPROPERTY(EditAnywhere, Category = "Gut|Glue", meta = (ClampMin = "0.05", ClampMax = "0.4"))
	float SinkHoldSeconds = 0.1f;

	/**
	 * Sink01/s while in contact even without move (~0.75s to full at 1.35).
	 * Fast glue: Prey_Rock Contained in ~0.6–1.2s with contact.
	 */
	UPROPERTY(EditAnywhere, Category = "Gut|Glue", meta = (ClampMin = "0.2", ClampMax = "4"))
	float SinkHoldRate = 1.45f;

	/** Extra Sink01/s from light tangential rub while contacting. */
	UPROPERTY(EditAnywhere, Category = "Gut|Glue", meta = (ClampMin = "0", ClampMax = "4"))
	float SinkRubBonus = 1.1f;

	UPROPERTY(EditAnywhere, Category = "Gut|Glue", meta = (ClampMin = "0", ClampMax = "4"))
	float SinkReleaseDecay = 1.4f;

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
		float Sink01 = 0.f;
		float SinkHoldAccum = 0.f;
		float ContainedAge = 0.f;
		float Grow01 = 0.f;
		float AbsorbRemaining = 8.f;
		bool bAbsorbNotified = false;
		bool bMassCommitted = false;
		float PhaseBob = 0.f;
		float Dissolve01 = 0.f;
	};

	void TickEntry(FGutEntry& Entry, float Dt);
	void TickHeldGlue(FGutEntry& Entry, float Dt);
	void EnterContained(FGutEntry& Entry);
	void FinishEntry(FGutEntry& Entry);
	ADevourable* FindPrimaryHeldPrey() const;
	void ComputeGlueContact(const ADevourable* Prey, bool& bOutContact, float& OutRub01) const;
	void GetPreyBounds(const ADevourable* Prey, FVector& OutCenter, float& OutRadiusXY, float& OutBottomZ, float& OutTopZ) const;

	TArray<FGutEntry> Entries;
};
