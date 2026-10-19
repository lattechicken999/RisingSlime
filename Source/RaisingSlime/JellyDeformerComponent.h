#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JellyDeformerComponent.generated.h"

/**
 * Baseline stub for the jelly spring body (spec §13).
 * Full procedural mesh + spring solver lands after expedition rules are stable.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAISINGSLIME_API UJellyDeformerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJellyDeformerComponent();

	void Step(float Dt);
	void ResetMotion();
	void AddImpulse(const FVector& LocalPoint, const FVector& LocalDirection, float Magnitude, float Radius, float Squash, float Stretch);

	bool IsReady() const { return false; }
};
