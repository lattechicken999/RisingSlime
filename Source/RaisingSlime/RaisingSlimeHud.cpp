#include "RaisingSlimeHud.h"
#include "SlimeHudWidget.h"
#include "DigestMenuWidget.h"
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
	CloseDigestMenu();
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

void ARaisingSlimeHUD::OpenDigestMenu()
{
	EnsureHudWidget();
	bDigestOpen = true;
	if (HudWidget)
	{
		HudWidget->SetDigestOpen(true);
	}

	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		PC->SetMenuOpen(true);
		if (!DigestWidget)
		{
			DigestWidget = CreateWidget<UDigestMenuWidget>(PC, UDigestMenuWidget::StaticClass());
		}
		if (DigestWidget)
		{
			DigestWidget->AddToViewport(50);
			DigestWidget->Refresh();
		}
	}
}

void ARaisingSlimeHUD::CloseDigestMenu()
{
	bDigestOpen = false;
	if (HudWidget)
	{
		HudWidget->SetDigestOpen(false);
	}
	if (DigestWidget)
	{
		DigestWidget->RemoveFromParent();
	}
	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		PC->SetMenuOpen(false);
		PC->SetCursorUnlocked(false);
	}
}
