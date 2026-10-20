#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FootOozePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ASlimePawn;

/**
 * Hunt pickup: enables ASlimePawn::bFootOozeFromItem (short-lived slime trail marks while moving).
 * Default WASD has no trail without this item. Respawns after cooldown.
 * Class name kept; effect is trail marks — not Fluids SW_Drop water.
 */
UCLASS()
class RAISINGSLIME_API AFootOozePickup : public AActor
{
	GENERATED_BODY()

public:
	AFootOozePickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float RespawnCooldown = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRespawnTimer();

	void Collect(ASlimePawn* Slime);
	void SetAvailable(bool bAvailable);

	bool bAvailable = true;
	FVector HomeLocation = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
};
