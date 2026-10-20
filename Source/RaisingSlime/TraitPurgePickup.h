#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TraitPurgePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Hunt world pickup: grants bHasTraitPurgeItem on overlap, then respawns after cooldown.
 * Tab trait delete stays gated on that flag. Cheat GiveTraitPurgeItem remains.
 */
UCLASS()
class RAISINGSLIME_API ATraitPurgePickup : public AActor
{
	GENERATED_BODY()

public:
	ATraitPurgePickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float RespawnCooldown = 18.f;

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

	void Collect();
	void SetAvailable(bool bAvailable);

	bool bAvailable = true;
	FVector HomeLocation = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
};
