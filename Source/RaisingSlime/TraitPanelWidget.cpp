#include "TraitPanelWidget.h"
#include "TraitPanelRowWidget.h"
#include "ExpeditionDirector.h"
#include "SlimePawn.h"
#include "SlimeGutContentComponent.h"
#include "RaisingSlimeHud.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

void UTraitPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildLayout();
	Refresh();
}

void UTraitPanelWidget::ClearChildren(UVerticalBox* Box)
{
	if (Box)
	{
		Box->ClearChildren();
	}
}

AExpeditionDirector* UTraitPanelWidget::FindDirector() const
{
	UWorld* World = GetWorld();
	return World
		? Cast<AExpeditionDirector>(UGameplayStatics::GetActorOfClass(World, AExpeditionDirector::StaticClass()))
		: nullptr;
}

ARaisingSlimeHUD* UTraitPanelWidget::FindHud() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return Cast<ARaisingSlimeHUD>(PC->GetHUD());
	}
	return nullptr;
}

void UTraitPanelWidget::RebuildLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.04f, 0.07f, 0.09f, 0.94f));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel);
		S->SetAnchors(FAnchors(1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetPosition(FVector2D(-24.f, 0.f));
		S->SetSize(FVector2D(420.f, 400.f));
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
	TitleText->SetText(FText::FromString(TEXT("형질")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = TitleText->GetFont();
		Font.Size = 20;
		TitleText->SetFont(Font);
	}
	AddPad(TitleText, 12.f);

	SlotLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLine"));
	SlotLine->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.85f, 0.9f)));
	AddPad(SlotLine);

	HintLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintLine"));
	HintLine->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.6f, 0.65f)));
	HintLine->SetText(FText::FromString(TEXT("위가 우선·활성. 드래그로 순서 변경.")));
	AddPad(HintLine, 6.f);

	ConfirmedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmedBox"));
	AddPad(ConfirmedBox, 10.f);

	SpitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SpitButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpitLabel"));
		Label->SetText(FText::FromString(TEXT("토해내기 (Q) — gut 우선 / 없으면 활성")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		SpitButton->AddChild(Label);
		SpitButton->OnClicked.AddDynamic(this, &UTraitPanelWidget::OnSpitClicked);
	}
	AddPad(SpitButton, 12.f);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
		Label->SetText(FText::FromString(TEXT("닫기 (Tab)")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CloseButton->AddChild(Label);
		CloseButton->OnClicked.AddDynamic(this, &UTraitPanelWidget::OnCloseClicked);
	}
	AddPad(CloseButton, 8.f);
}

void UTraitPanelWidget::AddTraitRow(ETraitId Trait, int32 Index)
{
	if (!ConfirmedBox)
	{
		return;
	}

	AExpeditionDirector* Director = FindDirector();
	const bool bCanPurge = Director && Director->HasTraitPurgeItem();
	const bool bIsActive = Index == 0;

	UTraitPanelRowWidget* Row = CreateWidget<UTraitPanelRowWidget>(this, UTraitPanelRowWidget::StaticClass());
	if (!Row)
	{
		return;
	}
	Row->Setup(this, Trait, Index, bCanPurge, bIsActive);
	if (UVerticalBoxSlot* VS = ConfirmedBox->AddChildToVerticalBox(Row))
	{
		VS->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}
}

void UTraitPanelWidget::Refresh()
{
	AExpeditionDirector* Director = FindDirector();
	if (!Director)
	{
		return;
	}

	if (SlotLine)
	{
		SlotLine->SetText(FText::FromString(FString::Printf(
			TEXT("확정 슬롯 %d/%d"), Director->GetConfirmedTraits().Num(), Director->GetSlotCapacity())));
	}
	if (HintLine)
	{
		HintLine->SetText(FText::FromString(Director->HasTraitPurgeItem()
			? TEXT("Q: gut 먹이 우선→없으면 Active. 드래그 순서. 삭제=아이템.")
			: TEXT("Q: gut 먹이 우선→없으면 Active. 드래그 순서. (삭제 아이템 없음)")));
	}
	if (SpitButton)
	{
		bool bGut = false;
		if (ASlimePawn* SoftSlime = Cast<ASlimePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
		{
			if (USlimeGutContentComponent* Gut = SoftSlime->GetGutContent())
			{
				bGut = Gut->HasSpittableGutPrey();
			}
		}
		SpitButton->SetIsEnabled(bGut || Director->GetConfirmedTraits().Num() > 0);
	}

	ClearChildren(ConfirmedBox);
	const TArray<ETraitId>& Confirmed = Director->GetConfirmedTraits();
	if (Confirmed.Num() == 0)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("확정된 형질 없음")));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
		ConfirmedBox->AddChildToVerticalBox(Empty);
	}
	else
	{
		for (int32 I = 0; I < Confirmed.Num(); ++I)
		{
			AddTraitRow(Confirmed[I], I);
		}
	}
}

void UTraitPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slot capacity can change as mass settles; keep header text live without rebuilding rows.
	if (AExpeditionDirector* Director = FindDirector())
	{
		if (SlotLine)
		{
			SlotLine->SetText(FText::FromString(FString::Printf(
				TEXT("확정 슬롯 %d/%d"), Director->GetConfirmedTraits().Num(), Director->GetSlotCapacity())));
		}
	}
}

bool UTraitPanelWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return InOperation && Cast<UTraitPanelRowWidget>(InOperation->Payload) != nullptr;
}

bool UTraitPanelWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UTraitPanelRowWidget* Dragged = InOperation ? Cast<UTraitPanelRowWidget>(InOperation->Payload) : nullptr;
	if (!Dragged || !ConfirmedBox)
	{
		return false;
	}

	const FVector2D Screen = InDragDropEvent.GetScreenSpacePosition();
	ETraitId DropTrait = ETraitId::None;
	float BestDist = TNumericLimits<float>::Max();

	for (int32 I = 0; I < ConfirmedBox->GetChildrenCount(); ++I)
	{
		if (UTraitPanelRowWidget* Row = Cast<UTraitPanelRowWidget>(ConfirmedBox->GetChildAt(I)))
		{
			const FGeometry& Geo = Row->GetCachedGeometry();
			const FVector2D Center = Geo.GetAbsolutePosition() + Geo.GetAbsoluteSize() * 0.5f;
			const float Dist = FMath::Abs(Center.Y - Screen.Y);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				DropTrait = Row->GetTrait();
			}
		}
	}

	if (DropTrait != ETraitId::None && DropTrait != Dragged->GetTrait())
	{
		HandleReorder(Dragged->GetTrait(), DropTrait);
		return true;
	}
	return false;
}

void UTraitPanelWidget::HandlePurge(ETraitId Trait)
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->PurgeConfirmedTrait(Trait);
		Refresh();
	}
}

void UTraitPanelWidget::HandleReorder(ETraitId Dragged, ETraitId DropTarget)
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->ReorderConfirmed(Dragged, DropTarget);
		Refresh();
	}
}

void UTraitPanelWidget::OnSpitClicked()
{
	if (AExpeditionDirector* Director = FindDirector())
	{
		Director->SpitActive();
		Refresh();
	}
}

void UTraitPanelWidget::OnCloseClicked()
{
	if (ARaisingSlimeHUD* Hud = FindHud())
	{
		Hud->CloseTraitPanel();
	}
}
