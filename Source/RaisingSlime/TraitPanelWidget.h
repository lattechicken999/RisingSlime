#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "TraitPanelWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class AExpeditionDirector;
class ARaisingSlimeHUD;
class UTraitPanelRowWidget;

/** Tab trait panel: Confirmed list, drag reorder, purge when item held. */
UCLASS()
class RAISINGSLIME_API UTraitPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	void Refresh();
	void HandlePurge(ETraitId Trait);
	void HandleReorder(ETraitId Dragged, ETraitId DropTarget);

protected:
	void RebuildLayout();
	void ClearChildren(UVerticalBox* Box);
	void AddTraitRow(ETraitId Trait, int32 Index);
	AExpeditionDirector* FindDirector() const;
	ARaisingSlimeHUD* FindHud() const;

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnSpitClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY()
	TObjectPtr<UTextBlock> SlotLine;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintLine;
	UPROPERTY()
	TObjectPtr<UVerticalBox> ConfirmedBox;
	UPROPERTY()
	TObjectPtr<UButton> SpitButton;
	UPROPERTY()
	TObjectPtr<UButton> CloseButton;
};
