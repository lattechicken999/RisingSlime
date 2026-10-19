#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RaisingSlimePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class RAISINGSLIME_API ARaisingSlimePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARaisingSlimePlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	void SetMenuOpen(bool bOpen);
	bool IsMenuOpen() const { return bMenuOpen; }
	void SetCursorUnlocked(bool bUnlocked);
	bool IsCursorUnlocked() const { return bCursorUnlocked; }
	bool IsGameplayLocked() const { return bMenuOpen || bCursorUnlocked; }

	/** Build IMC/actions if needed and add mapping context when LocalPlayer exists. */
	void EnsureRuntimeInput();

	UInputMappingContext* GetMappingContext() const { return MappingContext; }
	UInputAction* GetMoveAction() const { return MoveAction; }
	UInputAction* GetLookAction() const { return LookAction; }
	UInputAction* GetJumpAction() const { return JumpAction; }
	UInputAction* GetAttackAction() const { return AttackAction; }
	UInputAction* GetTraitAction() const { return TraitAction; }
	UInputAction* GetCycleTraitAction() const { return CycleTraitAction; }
	UInputAction* GetDigestAction() const { return DigestAction; }
	UInputAction* GetCursorAction() const { return CursorAction; }
	UInputAction* GetFocusAction() const { return FocusAction; }

protected:
	void BuildRuntimeInput();
	void ApplyMappingContext();
	void ApplyCursorState();
	void NotifyPossessedPawnToBindInput();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> AttackAction;
	UPROPERTY()
	TObjectPtr<UInputAction> TraitAction;
	UPROPERTY()
	TObjectPtr<UInputAction> CycleTraitAction;
	UPROPERTY()
	TObjectPtr<UInputAction> DigestAction;
	UPROPERTY()
	TObjectPtr<UInputAction> CursorAction;
	UPROPERTY()
	TObjectPtr<UInputAction> FocusAction;

	bool bMenuOpen = false;
	bool bCursorUnlocked = false;
};
