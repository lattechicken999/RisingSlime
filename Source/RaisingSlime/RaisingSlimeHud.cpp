#include "RaisingSlimeHud.h"
#include "SlimeHudWidget.h"
#include "TraitPanelWidget.h"
#include "RaisingSlimePlayerController.h"
#include "Blueprint/UserWidget.h"

ARaisingSlimeHUD::ARaisingSlimeHUD()
{
}

void ARaisingSlimeHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureHudWidget();
}

void ARaisingSlimeHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseTraitPanel();
	if (HudWidget)
	{
		HudWidget->RemoveFromParent();
		HudWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ARaisingSlimeHUD::EnsureHudWidget()
{
	if (HudWidget || !PlayerOwner)
	{
		return;
	}

	HudWidget = CreateWidget<USlimeHudWidget>(PlayerOwner, USlimeHudWidget::StaticClass());
	if (HudWidget)
	{
		HudWidget->AddToViewport(10);
		UE_LOG(LogTemp, Log, TEXT("RaisingSlimeHUD: UMG SlimeHudWidget attached"));
	}
}

void ARaisingSlimeHUD::OpenTraitPanel()
{
	EnsureHudWidget();
	bTraitPanelOpen = true;
	if (HudWidget)
	{
		HudWidget->SetTraitPanelOpen(true);
	}

	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		// Cursor for drag; do not SetMenuOpen — move/look stay available.
		PC->SetCursorUnlocked(true);
		if (!TraitPanelWidget)
		{
			TraitPanelWidget = CreateWidget<UTraitPanelWidget>(PC, UTraitPanelWidget::StaticClass());
		}
		if (TraitPanelWidget)
		{
			TraitPanelWidget->AddToViewport(50);
			TraitPanelWidget->Refresh();
		}
	}
}

void ARaisingSlimeHUD::CloseTraitPanel()
{
	bTraitPanelOpen = false;
	if (HudWidget)
	{
		HudWidget->SetTraitPanelOpen(false);
	}
	if (TraitPanelWidget)
	{
		TraitPanelWidget->RemoveFromParent();
	}
	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		PC->SetCursorUnlocked(false);
	}
}

void ARaisingSlimeHUD::ToggleTraitPanel()
{
	if (bTraitPanelOpen)
	{
		CloseTraitPanel();
	}
	else
	{
		OpenTraitPanel();
	}
}
