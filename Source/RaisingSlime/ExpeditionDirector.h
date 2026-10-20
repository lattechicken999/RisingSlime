#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaisingSlimeTypes.h"
#include "ExpeditionDirector.generated.h"

class ASlimePawn;
class ADevourable;

UCLASS()
class RAISINGSLIME_API AExpeditionDirector : public AActor
{
	GENERATED_BODY()

public:
	AExpeditionDirector();

	virtual void BeginPlay() override;

	void RegisterSlime(ASlimePawn* InSlime);
	ASlimePawn* GetSlime() const { return Slime; }

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	float GetSettledMass() const { return SettledMass; }

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	float GetRunMass() const { return RunMass; }

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	float GetTotalMass() const { return SettledMass + RunMass; }

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	int32 GetSlotCapacity() const { return 1 + FMath::FloorToInt(SettledMass / 10.f); }

	ETraitId GetActiveTrait() const { return Active; }
	bool IsActiveConfirmed() const;
	const TArray<ETraitId>& GetConfirmedTraits() const { return Confirmed; }
	const TArray<ETraitId>& GetUnstableTraits() const { return Unstable; }
	const FString& GetStatus() const { return Status; }

	bool HasTraitPurgeItem() const { return bHasTraitPurgeItem; }
	void SetHasTraitPurgeItem(bool bValue) { bHasTraitPurgeItem = bValue; }

	void CopyOwned(TArray<ETraitId>& OutOwned) const;
	float PreviewChance(const ADevourable* Prey) const;

	UFUNCTION(BlueprintCallable, Category = "Expedition")
	void CompleteDevour(ADevourable* Prey);

	/** Contained seal: commit run mass (growth starts after grow ease). */
	void OnGutContained(float Mass, ETraitId Trait, ADevourable* Prey);

	/** Field absorb: settle one gut's mass; promote Unstable→Confirmed if slot free. */
	void OnGutAbsorbed(float Mass, ETraitId Trait, ADevourable* Prey);

	void CycleActive();
	bool TryConfirm(ETraitId Trait);
	void Spit(ETraitId Trait);
	bool PurgeConfirmedTrait(ETraitId Trait);
	bool ReorderConfirmed(ETraitId Dragged, ETraitId DropTarget);
	void SyncActiveFromConfirmed();

	/** Legacy nest settle — unused by field absorb UI; kept for tooling. */
	UFUNCTION(BlueprintCallable, Category = "Expedition")
	void Settle();

	void Die();
	void ResetWorld();
	void TeleportSlimeToPlayerStart();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest")
	FTransform NestSpawnTransform;

protected:
	bool Owns(ETraitId Trait) const;

	UPROPERTY()
	TObjectPtr<ASlimePawn> Slime;

	float SettledMass = 0.f;
	float RunMass = 0.f;
	ETraitId Active = ETraitId::None;
	TArray<ETraitId> Confirmed;
	TArray<ETraitId> Unstable;
	TMap<ETraitId, int32> Pity;
	float LastEatTime = 0.f;
	bool bHasTraitPurgeItem = false;
	FString Status = TEXT("사물과 쓰러뜨린 개체를 삼켜라. Tab으로 형질을 본다.");
};
