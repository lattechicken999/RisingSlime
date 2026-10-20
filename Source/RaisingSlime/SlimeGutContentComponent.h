#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaisingSlimeTypes.h"
#include "SlimeGutContentComponent.generated.h"

class ADevourable;
class USlimeEngulfVenomComponent;

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
 * Natural devour (alt B′): Held + press-meter Engulf01 + venom ribbons → Contained seal float.
 * No center pull. Soft-volume climb sleeve is not Contained authority.
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
	float GetPrimaryEngulf01() const;

	FOnGutAbsorbed OnGutAbsorbed;
	FOnGutContained OnGutContained;

	UPROPERTY(EditAnywhere, Category = "Gut|Absorb")
	float AbsorbDuration = 8.f;

	/** Contained when press-meter reaches this. */
	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float EngulfCompleteThreshold = 0.98f;

	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float EngulfHoldSeconds = 0.12f;

	/** Engulf01 charge while Contact && Press (1/s). */
	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0.1", ClampMax = "3"))
	float EngulfChargeRate = 0.9f;

	/** Slow leak while touching but not pressing. */
	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0", ClampMax = "2"))
	float EngulfIdleDecay = 0.12f;

	/** Faster drop when contact lost. */
	UPROPERTY(EditAnywhere, Category = "Gut|Engulf", meta = (ClampMin = "0", ClampMax = "3"))
	float EngulfReleaseDecay = 0.55f;

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
		float Engulf01 = 0.f;
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
	void SyncVenomPresentation();
	ADevourable* FindPrimaryHeldPrey() const;
	USlimeEngulfVenomComponent* GetVenom() const;
	void ComputePressContact(const ADevourable* Prey, float& OutPress, bool& bOutContact) const;
	void TickVenomDemo(float Dt);

	TArray<FGutEntry> Entries;
	bool bVenomDemo = false;
	float VenomDemoAge = 0.f;
	float VenomDemoQuitAccum = 0.f;
	int32 VenomDemoShot = 0;
};
