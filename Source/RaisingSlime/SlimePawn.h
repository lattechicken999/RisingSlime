#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RaisingSlimeTypes.h"
#include "SlimePawn.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USlimeMovementComponent;
class UJellyDeformerComponent;
class USlimeSoftVolumeComponent;
class USlimeGutContentComponent;
class USlimeEngulfVenomComponent;
class USlimeFleshExperimentComponent;
class UMaterialInterface;
class AExpeditionDirector;
class ADevourable;
class ARaisingSlimeHUD;
class UInputComponent;

UCLASS()
class RAISINGSLIME_API ASlimePawn : public APawn
{
	GENERATED_BODY()

public:
	ASlimePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	USphereComponent* GetCollisionSphere() const { return Collision; }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	UCameraComponent* GetMainCamera() const { return MainCamera; }

	USlimeMovementComponent* GetSlimeMovement() const { return Movement; }
	USlimeSoftVolumeComponent* GetSoftVolume() const { return SoftVolume; }
	USlimeGutContentComponent* GetGutContent() const { return GutContent; }
	USlimeEngulfVenomComponent* GetEngulfVenom() const { return EngulfVenom; }
	UMaterialInterface* GetCachedSlimeMaterial() const { return CachedSlimeMaterial; }

	/** Begin melting swallowed proxies (death / legacy settle). */
	void BeginGutDissolve();
	void ClearGutContent();
	void HandleGutContained(float Mass, ETraitId Trait, ADevourable* Prey);
	void HandleGutAbsorbed(float Mass, ETraitId Trait, ADevourable* Prey);

	UFUNCTION(BlueprintCallable, Category = "Slime")
	float GetHp() const { return Hp; }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	float GetMaxHp() const { return MaxHp; }

	float GetChannel01() const { return Channel01; }
	float GetViewYaw() const { return Yaw; }

	UFUNCTION(BlueprintCallable, Category = "Slime")
	float GetViewPitch() const { return Pitch; }

	AActor* GetEngulfTarget() const;
	AActor* GetAimedPrey() const;

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void RefreshBody();
	void NotifyAte();
	void Recover();
	void TeleportToSpawn(const FTransform& SpawnTransform);
	void TakeDamageAmount(float Amount);
	void AddImpact(float Amount);
	void PlayDevourSplash(const FVector& AtLocation);
	void PlaySpitSplash();
	void PlayDeathSplash();

	float GetBodySize() const;

	/** Bind Enhanced Input actions from the player controller. Safe to call multiple times. */
	void BindEnhancedInput();

protected:
	void OnMove(const struct FInputActionValue& Value);
	void OnLook(const struct FInputActionValue& Value);
	void OnJumpStarted(const struct FInputActionValue& Value);
	void OnAttack(const struct FInputActionValue& Value);
	void OnTrait(const struct FInputActionValue& Value);
	void OnCycleTrait(const struct FInputActionValue& Value);
	void OnTraitPanel(const struct FInputActionValue& Value);
	void OnCursor(const struct FInputActionValue& Value);
	void OnFocus(const struct FInputActionValue& Value);

	void ApplyLookFromRawPixels(const FVector2D& PixelDelta);
	void UpdateAim();
	void Devour(float DeltaSeconds);
	void Attack();
	void UseTrait();
	void ApplyBodySize();
	float ReachMeters() const;
	AExpeditionDirector* FindDirector() const;
	bool IsMenuBlocking() const;
	bool IsCombatBlocked() const;
	ADevourable* FindEngulfTarget() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UCameraComponent> MainCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UJellyDeformerComponent> Jelly;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeSoftVolumeComponent> SoftVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeGutContentComponent> GutContent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeEngulfVenomComponent> EngulfVenom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeFleshExperimentComponent> FleshExperiment;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CachedSlimeMaterial;

	float Hp = 100.f;
	float MaxHp = 100.f;
	float Yaw = 0.f;
	float Pitch = RaisingSlimeUnits::DefaultPitch;
	float FixedAccumulator = 0.f;
	float NextAttackTime = 0.f;
	float Channel = 0.f;
	float Channel01 = 0.f;
	float HardenUntil = 0.f;
	float HardenCooldown = 0.f;
	bool bJumpQueued = false;
	bool bEnhancedInputBound = false;
	bool bLoggedMoveInput = false;

	FVector2D MoveInput = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<ADevourable> EngulfTarget;

	UPROPERTY()
	TObjectPtr<ADevourable> Aimed;
};
