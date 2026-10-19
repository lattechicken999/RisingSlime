#include "DigestMenuWidget.h"
#include "DigestTraitRowWidget.h"
#include "ExpeditionDirector.h"
#include "RaisingSlimeHud.h"
#include "RaisingSlimePlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

void UDigestMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildLayout();
	Refresh();
}

void UDigestMenuWidget::ClearChildren(UVerticalBox* Box)
{
	if (Box)
	{
		Box->ClearChildren();
	}
}

AExpeditionDirector* UDigestMenuWidget::FindDirector() const
{
	UWorld* World = GetWorld();
	return World
		? Cast<AExpeditionDirector>(UGameplayStatics::GetActorOfClass(World, AExpeditionDirector::StaticClass()))
		: nullptr;
}

ARaisingSlimeHUD* UDigestMenuWidget::FindHud() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return Cast<ARaisingSlimeHUD>(PC->GetHUD());
	}
	return nullptr;
}

void UDigestMenuWidget::RebuildLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.05f, 0.08f, 0.10f, 0.92f));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(460.f, 380.f));
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	auto AddPad = [&](UWidget* Child, float PadTop = 4.f)
	{
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Child))
		{
			VS->SetPadding(FMargin(16.f, PadTop, 16.f, 0.f));
			VS->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetText(FText::FromString(TEXT("소화실")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = TitleText->GetFont();
		Font.Size = 20;
		TitleText->SetFont(Font);
	}
	AddPad(TitleText, 12.f);

	MassLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MassLine"));
	MassLine->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddPad(MassLine);

	SlotLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLine"));
	SlotLine->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddPad(SlotLine);

	UTextBlock* UnstableHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnstableHeader"));
	UnstableHeader->SetText(FText::FromString(TEXT("불안정")));
	UnstableHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.4f)));
	AddPad(UnstableHeader, 12.f);

	UnstableBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UnstableBox"));
	AddPad(UnstableBox);

	UTextBlock* ConfirmedHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmedHeader"));
	ConfirmedHeader->SetText(FText::FromString(TEXT("확정")));
	ConfirmedHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.85f, 1.f)));
	AddPad(ConfirmedHeader, 12.f);

	ConfirmedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmedBox"));
	AddPad(ConfirmedBox);

	SettleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SettleButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettleLabel"));
		Label->SetText(FText::FromString(TEXT("질량 정착하고 먹이 되돌리기")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		SettleButton->AddChild(Label);
		SettleButton->OnClicked.AddDynamic(this, &UDigestMenuWidget::OnSettleClicked);
	}
	AddPad(SettleButton, 16.f);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
		Label->SetText(FText::FromString(TEXT("닫기")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CloseButton->AddChild(Label);
		CloseButton->OnClicked.AddDynamic(this, &UDigestMenuWidget::OnCloseClicked);
	}
	AddPad(CloseButton, 8.f);
}

void UDigestMenuWidget::AddTraitRow(UVerticalBox* Box, ETraitId Trait, bool bConfirmButton)
{
	if (!Box)
	{
		return;
	}

	UDigestTraitRowWidget* Row = CreateWidget<UDigestTraitRowWidget>(this, UDigestTraitRowWidget::StaticClass());
	if (!Row)
	{
		return;
	}
	Row->Setup(this, Trait, bConfirmButton);
	if (UVerticalBoxSlot* VS = Box->AddChildToVerticalBox(Row))
	{
		VS->SetPadding(FMargin(8.f, 2.f, 0.f, 2.f));
	}
}

void UDigestMenuWidget::Refresh()
{
	AExpeditionDirector* Director = FindDirector();
	if (!Director)
	{
		return;
	}

	if (MassLine)
	{
		MassLine->SetText(FText::FromString(FString::Printf(
			TEXT("이번 출정 질량 %.1f  /  정착 %.1f"), Director->GetRunMass(), Director->GetSettledMass())));
	}
	if (SlotLine)
	{
		SlotLine->SetText(FText::FromString(FString::Printf(
			TEXT("확정 슬롯 %d/%d"), Director->GetConfirmedTraits().Num(), Director->GetSlotCapacity())));
	}

	ClearChildren(UnstableBox);
	if (Director->GetUnstableTraits().Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("없음")));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
		UnstableBox->AddChildToVerticalBox(Empty);
	}
	else
	{
		for (ETraitId Trait : Director->GetUnstableTraits())
		{
			AddTraitRow(UnstableBox, Trait, true);
		}
	}

	ClearChildren(ConfirmedBox);
	if (Director->GetConfirmedTraits().Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("없음")));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
		ConfirmedBox->AddChildToVerticalBox(Empty);
	}
	else
	{
		for (ETraitId Trait : Director->GetConfirmedTraits())
		{
			AddTraitRow(ConfirmedBox, Trait, false);
		}
	}
}

void UDigestMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(GetOwningPlayer());
	AExpeditionDirector* Director = FindDirector();
	if (!PC || !Director)
	{
		return;
	}

	// Keyboard parity with previous canvas digest (1/2/3/Esc).
	if (PC->WasInputKeyJustPressed(EKeys::One) && Director->GetUnstableTraits().Num() > 0)
	{
		HandleConfirm(Director->GetUnstableTraits()[0]);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Two) && Director->GetConfirmedTraits().Num() > 0)
	{
		HandleSpit(Director->GetConfirmedTraits()[0]);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Three))
	{
		OnSettleClicked();
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		OnCloseClicked();
	}
}

void UDigestMenuWidget::OnSettleClicked()
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->Settle();
	}
	OnCloseClicked();
}

void UDigestMenuWidget::OnCloseClicked()
{
	if (ARaisingSlimeHUD* Hud = FindHud())
	{
		Hud->CloseDigestMenu();
	}
}

void UDigestMenuWidget::HandleConfirm(ETraitId Trait)
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->TryConfirm(Trait);
		Refresh();
	}
}

void UDigestMenuWidget::HandleSpit(ETraitId Trait)
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->Spit(Trait);
		Refresh();
	}
}
