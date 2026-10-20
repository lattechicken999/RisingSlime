#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatTarget.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UCombatTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * C0 combat contract: Health / Downed / devour gate.
 * Living combatants must not start surface-drift devour.
 */
class RAISINGSLIME_API ICombatTarget
{
	GENERATED_BODY()

public:
	virtual float GetCombatHp() const = 0;
	virtual float GetCombatMaxHp() const = 0;
	virtual bool IsCombatDowned() const = 0;
	/** True when Downed (hostile) or non-hostile prey always devourable via ADevourable. */
	virtual bool CanBeDevoured() const = 0;
	virtual void ApplyDamage(float Amount, AActor* Instigator, FName DamageTag) = 0;
	virtual void EnterDowned() = 0;
};
