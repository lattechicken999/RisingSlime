#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeHudWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UBorder;
class AExpeditionDirector;
class ASlimePawn;

/** Spec §15 gameplay HUD (WBP_SlimeHud equivalent, built in native C++). */
UCLASS()
class RAISINGSLIME_API USlimeHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetDigestOpen(bool bOpen) { bDigestOpen = bOpen; }

protected:
	void RebuildLayout();
	void RefreshFromWorld();

	UPROPERTY()
	TObjectPtr<UProgressBar> HpBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> HpText;
	UPROPERTY()
	TObjectPtr<UTextBlock> MassText;
	UPROPERTY()
	TObjectPtr<UTextBlock> SlotText;
	UPROPERTY()
	TObjectPtr<UTextBlock> ActiveText;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;
	UPROPERTY()
	TObjectPtr<UBorder> CrossH;
	UPROPERTY()
	TObjectPtr<UBorder> CrossV;
	UPROPERTY()
	TObjectPtr<UTextBlock> AimText;
	UPROPERTY()
	TObjectPtr<UProgressBar> ChannelBar;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintBottom;
	UPROPERTY()
	TObjectPtr<UTextBlock> NestHint;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	bool bDigestOpen = false;
};
