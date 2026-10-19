#include "DigestTraitRowWidget.h"
#include "DigestMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

void UDigestTraitRowWidget::Setup(UDigestMenuWidget* OwnerMenu, ETraitId InTrait, bool bInConfirm)
{
	Menu = OwnerMenu;
	Trait = InTrait;
	bConfirm = bInConfirm;
	if (NameText)
	{
		NameText->SetText(FText::FromString(TraitText::Name(Trait)));
	}
	if (ActionButton && ActionButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(ActionButton->GetChildAt(0)))
		{
			Label->SetText(FText::FromString(bConfirm ? TEXT("확정") : TEXT("토해내기")));
		}
	}
}

void UDigestTraitRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	WidgetTree->RootWidget = Row;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameText->SetText(FText::FromString(TraitText::Name(Trait)));
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(NameText))
	{
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HS->SetVerticalAlignment(VAlign_Center);
	}

	ActionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Action"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionLabel"));
	Label->SetText(FText::FromString(bConfirm ? TEXT("확정") : TEXT("토해내기")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ActionButton->AddChild(Label);
	ActionButton->OnClicked.AddDynamic(this, &UDigestTraitRowWidget::OnActionClicked);
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(ActionButton))
	{
		HS->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
}

void UDigestTraitRowWidget::OnActionClicked()
{
	if (!Menu)
	{
		return;
	}
	if (bConfirm)
	{
		Menu->HandleConfirm(Trait);
	}
	else
	{
		Menu->HandleSpit(Trait);
	}
}
