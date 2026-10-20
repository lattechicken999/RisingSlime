#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaisingSlimeTypes.h"
#include "TraitPanelRowWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UTraitPanelWidget;

UCLASS()
class RAISINGSLIME_API UTraitPanelRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UTraitPanelWidget* OwnerPanel, ETraitId InTrait, int32 InIndex, bool bCanPurge, bool bIsActive);

	ETraitId GetTrait() const { return Trait; }
	int32 GetListIndex() const { return ListIndex; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	UFUNCTION()
	void OnPurgeClicked();

	UPROPERTY()
	TObjectPtr<UTraitPanelWidget> Panel;
	UPROPERTY()
	TObjectPtr<UBorder> RowBorder;
	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY()
	TObjectPtr<UTextBlock> RankText;
	UPROPERTY()
	TObjectPtr<UButton> PurgeButton;

	ETraitId Trait = ETraitId::None;
	int32 ListIndex = 0;
	bool bCanPurge = false;
	bool bIsActive = false;
};
