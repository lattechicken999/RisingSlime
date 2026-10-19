#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeShot.generated.h"

class ASlimePawn;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class RAISINGSLIME_API ASlimeShot : public AActor
{
	GENERATED_BODY()

public:
	ASlimeShot();

	virtual void Tick(float DeltaSeconds) override;

	void Launch(ASlimePawn* InOwner, const FVector& Direction, float InDamage, bool bInPoison, float InPoisonDps, float InPoisonDuration);

protected:
	bool TryHit(float StepCm);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY()
	TObjectPtr<ASlimePawn> ShotOwner;

	FVector MoveDirection = FVector::ForwardVector;
	float Damage = 14.f;
	bool bPoison = false;
	float PoisonDps = 0.f;
	float PoisonDuration = 0.f;
	float Life = 2.2f;
	static constexpr float SpeedCm = 1800.f;
	static constexpr float RadiusCm = 16.f;
};
