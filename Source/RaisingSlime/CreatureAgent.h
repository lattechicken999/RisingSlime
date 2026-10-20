#pragma once

#include "CoreMinimal.h"
#include "CombatTarget.h"
#include "Devourable.h"
#include "CreatureAgent.generated.h"

class ASlimePawn;
class UCapsuleComponent;

UCLASS()
class RAISINGSLIME_API ACreatureAgent : public ADevourable, public ICombatTarget
{
	GENERATED_BODY()

public:
	ACreatureAgent();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestoreScenePlacement() override;

	// ICombatTarget
	virtual float GetCombatHp() const override { return Hp; }
	virtual float GetCombatMaxHp() const override { return MaxHp; }
	virtual bool IsCombatDowned() const override { return bDowned; }
	virtual bool CanBeDevoured() const override;
	virtual void ApplyDamage(float Amount, AActor* Instigator, FName DamageTag) override;
	virtual void EnterDowned() override;

	UFUNCTION(BlueprintCallable, Category = "Creature")
	bool IsDowned() const { return bDowned; }

	UFUNCTION(BlueprintCallable, Category = "Creature")
	float GetHp() const { return Hp; }

	/** Legacy hit path (poison DoT — C1 uses ApplyDamage + separate DoT). */
	void TakeHit(float Damage, bool bApplyPoison, float Dps, float Duration);
	void Revive();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float MaxHp = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float MoveSpeed = 3.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float AttackDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float AttackInterval = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float AttackRange = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Creature")
	float NoticeRange = 18.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Creature")
	TObjectPtr<UCapsuleComponent> Capsule;

protected:
	void TickPoison(float DeltaSeconds);
	void MoveTowards(const FVector& Destination, float DeltaSeconds);
	void ApplyTint(FLinearColor Color);
	void ClearTint();

	UPROPERTY()
	TObjectPtr<ASlimePawn> Player;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMat;

	FVector HomeLocation;
	float Hp = 36.f;
	float FallVelocity = -200.f;
	float NextAttack = 0.f;
	float PoisonDps = 0.f;
	float PoisonUntil = 0.f;
	bool bDowned = false;
};
