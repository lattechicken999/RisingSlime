#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaisingSlimeTypes.h"
#include "SlimeGutContentComponent.generated.h"

class ADevourable;
class UStaticMeshComponent;

DECLARE_DELEGATE_ThreeParams(FOnGutAbsorbed, float /*Mass*/, ETraitId /*Trait*/, ADevourable* /*Prey*/);

/**
 * Gut visuals: swallowed prey floats inside the translucent slime until absorb timer
 * completes, then dissolves. Does not drive soft-volume physics.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API USlimeGutContentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeGutContentComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pull prey mesh into the body as a floating gut proxy. */
	void Swallow(ADevourable* Prey);

	/** Start melting all gut contents (death). */
	void BeginDissolveAll();

	void ClearImmediate();

	int32 GetGutCount() const { return Entries.Num(); }
	float GetSwallowSwellBonus() const { return SwellBonus; }

	FOnGutAbsorbed OnGutAbsorbed;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float AbsorbDuration = 8.f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float BobAmpCm = 6.f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float BobSpeed = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float FitScale = 0.38f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float DissolveSeconds = 2.4f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float PullInSeconds = 0.45f;

	/** Extra body-size swell on swallow so growth reads immediately. */
	UPROPERTY(EditAnywhere, Category = "Gut")
	float SwallowSwell = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Gut")
	float SwellDecayPerSec = 0.55f;

protected:
	struct FGutEntry
	{
		UStaticMeshComponent* Mesh = nullptr;
		TWeakObjectPtr<ADevourable> Prey;
		float Mass = 0.f;
		ETraitId Trait = ETraitId::None;
		float AbsorbRemaining = 8.f;
		bool bAbsorbNotified = false;
		FVector BaseLocal = FVector::ZeroVector;
		FVector PullStartLocal = FVector::ZeroVector;
		float Phase = 0.f;
		float Pull01 = 0.f;
		float Dissolve01 = 0.f;
		bool bPulling = true;
		bool bDissolving = false;
		float RestScale = 1.f;
	};

	void TickEntry(FGutEntry& Entry, float Dt);
	FVector GutAnchorLocal() const;

	TArray<FGutEntry> Entries;
	float SwellBonus = 0.f;
};
