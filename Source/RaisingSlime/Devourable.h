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

	void StoreHome();
	void Consume();
	/** Start per-actor respawn after absorb (or death cleanup). */
	void BeginRespawnCooldown();
	virtual void RestoreScenePlacement();

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

	/** Seconds after absorb before this prey returns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prey")
	float RespawnCooldown = 12.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prey")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	UFUNCTION()
	void OnRespawnTimer();

	bool bConsumed = false;
	bool bHomeStored = false;
	FTransform HomeTransform;
	FTimerHandle RespawnTimer;
};
