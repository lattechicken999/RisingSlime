#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaisingSlimeHud.generated.h"

class AExpeditionDirector;
class ASlimePawn;
class ARaisingSlimePlayerController;

UCLASS()
class RAISINGSLIME_API ARaisingSlimeHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARaisingSlimeHUD();

	virtual void DrawHUD() override;

	void OpenDigestMenu();
	void CloseDigestMenu();
	bool IsDigestMenuOpen() const { return bDigestOpen; }

protected:
	void DrawDigestPanel(AExpeditionDirector* Director, ARaisingSlimePlayerController* PC);
	void DrawStatusBars(AExpeditionDirector* Director, ASlimePawn* Slime);

	bool bDigestOpen = false;
	int32 DigestHotIndex = -1;
};
