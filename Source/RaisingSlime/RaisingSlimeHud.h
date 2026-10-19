#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaisingSlimeHud.generated.h"

class USlimeHudWidget;
class UDigestMenuWidget;

UCLASS()
class RAISINGSLIME_API ARaisingSlimeHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARaisingSlimeHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenDigestMenu();
	void CloseDigestMenu();
	bool IsDigestMenuOpen() const { return bDigestOpen; }

protected:
	void EnsureHudWidget();

	UPROPERTY()
	TObjectPtr<USlimeHudWidget> HudWidget;

	UPROPERTY()
	TObjectPtr<UDigestMenuWidget> DigestWidget;

	bool bDigestOpen = false;
};
