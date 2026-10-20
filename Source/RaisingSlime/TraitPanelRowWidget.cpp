#include "TraitPanelRowWidget.h"
#include "TraitPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

void UTraitPanelRowWidget::Setup(UTraitPanelWidget* OwnerPanel, ETraitId InTrait, int32 InIndex, bool bInCanPurge, bool bInIsActive)
{
	Panel = OwnerPanel;
	Trait = InTrait;
	ListIndex = InIndex;
	bCanPurge = bInCanPurge;
	bIsActive = bInIsActive;

	if (NameText)
	{
		const FString Label = bIsActive
			? FString::Printf(TEXT("%s  (활성)"), *TraitText::Name(Trait))
			: TraitText::Name(Trait);
		NameText->SetText(FText::FromString(Label));
		NameText->SetColorAndOpacity(FSlateColor(bIsActive
			? FLinearColor(0.55f, 0.95f, 0.75f)
			: FLinearColor::White));
	}
	if (RankText)
	{
		RankText->SetText(FText::FromString(FString::Printf(TEXT("%d"), ListIndex + 1)));
	}
	if (PurgeButton)
	{
		PurgeButton->SetVisibility(bCanPurge ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PurgeButton->SetIsEnabled(bCanPurge);
	}
	if (RowBorder)
	{
		RowBorder->SetBrushColor(bIsActive
			? FLinearColor(0.12f, 0.28f, 0.22f, 0.95f)
			: FLinearColor(0.08f, 0.10f, 0.12f, 0.9f));
	}
}

void UTraitPanelRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowBorder"));
	RowBorder->SetBrushColor(FLinearColor(0.08f, 0.10f, 0.12f, 0.9f));
	RowBorder->SetPadding(FMargin(8.f, 6.f));
	WidgetTree->RootWidget = RowBorder;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	RowBorder->SetContent(Row);

	RankText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Rank"));
	RankText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.7f, 0.75f)));
	RankText->SetText(FText::FromString(FString::Printf(TEXT("%d"), ListIndex + 1)));
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(RankText))
	{
		HS->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameText->SetText(FText::FromString(TraitText::Name(Trait)));
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(NameText))
	{
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	PurgeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Purge"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PurgeLabel"));
		Label->SetText(FText::FromString(TEXT("삭제")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PurgeButton->AddChild(Label);
		PurgeButton->OnClicked.AddDynamic(this, &UTraitPanelRowWidget::OnPurgeClicked);
		PurgeButton->SetVisibility(bCanPurge ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(PurgeButton))
	{
		HS->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
}

FReply UTraitPanelRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTraitPanelRowWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	UDragDropOperation* Op = NewObject<UDragDropOperation>(this);
	Op->Payload = this;
	Op->DefaultDragVisual = this;
	Op->Pivot = EDragPivot::MouseDown;
	OutOperation = Op;
}

void UTraitPanelRowWidget::OnPurgeClicked()
{
	if (Panel && bCanPurge)
	{
		Panel->HandlePurge(Trait);
	}
}
