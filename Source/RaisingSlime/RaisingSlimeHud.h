#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaisingSlimeHud.generated.h"

class USlimeHudWidget;
class UTraitPanelWidget;

UCLASS()
class RAISINGSLIME_API ARaisingSlimeHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARaisingSlimeHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenTraitPanel();
	void CloseTraitPanel();
	void ToggleTraitPanel();
	bool IsTraitPanelOpen() const { return bTraitPanelOpen; }

protected:
	void EnsureHudWidget();

	UPROPERTY()
	TObjectPtr<USlimeHudWidget> HudWidget;

	UPROPERTY()
	TObjectPtr<UTraitPanelWidget> TraitPanelWidget;

	bool bTraitPanelOpen = false;
};
