#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "DigestMenuWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class AExpeditionDirector;
class ARaisingSlimeHUD;

/** Spec §15 digest room (WBP_Digest equivalent). Buttons call Director; lists rebuilt from live state. */
UCLASS()
class RAISINGSLIME_API UDigestMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void Refresh();
	void HandleConfirm(ETraitId Trait);
	void HandleSpit(ETraitId Trait);

protected:
	void RebuildLayout();
	void ClearChildren(UVerticalBox* Box);
	void AddTraitRow(UVerticalBox* Box, ETraitId Trait, bool bConfirmButton);
	AExpeditionDirector* FindDirector() const;
	ARaisingSlimeHUD* FindHud() const;

	UFUNCTION()
	void OnSettleClicked();
	UFUNCTION()
	void OnCloseClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY()
	TObjectPtr<UTextBlock> MassLine;
	UPROPERTY()
	TObjectPtr<UTextBlock> SlotLine;
	UPROPERTY()
	TObjectPtr<UVerticalBox> UnstableBox;
	UPROPERTY()
	TObjectPtr<UVerticalBox> ConfirmedBox;
	UPROPERTY()
	TObjectPtr<UButton> SettleButton;
	UPROPERTY()
	TObjectPtr<UButton> CloseButton;
};
