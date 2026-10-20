#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaisingSlimeTypes.h"
#include "Devourable.generated.h"

UCLASS()
class RAISINGSLIME_API ADevourable : public AActor
{
	GENERATED_BODY()

public:
	ADevourable();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Prey")
	FString GetLabel() const;

	UFUNCTION(BlueprintCallable, Category = "Prey")
	bool CanDevour() const;

	float GetChannelTime() const { return Kind == EPreyKind::Creature ? 0.85f : 0.22f; }

	UFUNCTION(BlueprintCallable, Category = "Prey")
	bool IsConsumed() const { return bConsumed; }

	bool IsInsideGut() const { return bInsideGut; }

	void StoreHome();

	/** Channel done: collision/AI off, stay visible at current world pose (Held/Engulf). */
	void BeginHeld();

	/** Contained seal: attach under slime with fixed local offset (no center pull). */
	void SealIntoGut(USceneComponent* GutRoot, const FVector& LocalOffset);

	/** Hide + detach after absorb dissolve or death; then start respawn timer. */
	void FinishGutAndRespawn();

	/** Spit from gut: place in world and optionally launch (no respawn timer). Re-devourable. */
	void ReleaseFromGutToWorld(const FVector& WorldLocation, const FVector& LaunchVelocity = FVector::ZeroVector);

	void BeginRespawnCooldown();
	virtual void RestoreScenePlacement();

	/** Collect center + AABB corner samples for engulf coverage. */
	void CollectEngulfSamplePoints(TArray<FVector>& OutPoints) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	EPreyKind Kind = EPreyKind::Object;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	float Mass = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	ETraitId Trait = ETraitId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	float BaseChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	float RespawnCooldown = 12.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prey")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	UFUNCTION()
	void OnRespawnTimer();

	void DisableCollisionKeepVisible();

	bool bConsumed = false;
	bool bInsideGut = false;
	bool bHomeStored = false;
	FTransform HomeTransform;
	FVector HomeScale3D = FVector::OneVector;
	FVector HeldWorldLocation = FVector::ZeroVector;
	FTimerHandle RespawnTimer;
};
