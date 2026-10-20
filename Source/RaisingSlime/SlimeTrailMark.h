#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeTrailMark.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Short-lived footprint-sized slime mark under contact.
 * No Fluids / shallow-water — does not spread or pool.
 */
UCLASS()
class RAISINGSLIME_API ASlimeTrailMark : public AActor
{
	GENERATED_BODY()

public:
	ASlimeTrailMark();

	virtual void Tick(float DeltaSeconds) override;

	/** Spawn a fading slime-colored disc at WorldLocation (cm). ScaleCm ≈ footprint diameter. */
	static ASlimeTrailMark* SpawnAt(
		UWorld* World,
		const FVector& WorldLocation,
		float ScaleCm,
		float LifetimeSeconds = 0.85f);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trail")
	TObjectPtr<UStaticMeshComponent> MarkMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MarkMid;

	float Lifetime = 0.85f;
	float Age = 0.f;
	float BaseScale = 1.f;
};
