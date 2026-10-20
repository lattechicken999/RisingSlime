#include "SlimeHudWidget.h"
#include "ExpeditionDirector.h"
#include "Devourable.h"
#include "CreatureAgent.h"
#include "RaisingSlimeTypes.h"
#include "SlimePawn.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, float Size = 16.f)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		return T;
	}

	void AnchorPx(UCanvasPanelSlot* Slot, float X, float Y, float W, float H, float PivotX = 0.f, float PivotY = 0.f)
	{
		// 1920x1080 design pixels → anchors
		Slot->SetAnchors(FAnchors(X / 1920.f, Y / 1080.f, (X + W) / 1920.f, (Y + H) / 1080.f));
		Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		Slot->SetAlignment(FVector2D(PivotX, PivotY));
	}
}

void USlimeHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildLayout();
}

void USlimeHudWidget::RebuildLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto AddAt = [&](UWidget* W, float X, float Y, float Wd, float Ht) -> UCanvasPanelSlot*
	{
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(W);
		AnchorPx(Slot, X, Y, Wd, Ht);
		return Slot;
	};

	UBorder* HpBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HpBg"));
	HpBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	AddAt(HpBg, 24, 20, 280, 18);

	HpBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HpBar"));
	HpBar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.18f, 0.20f, 1.f));
	HpBar->SetPercent(1.f);
	AddAt(HpBar, 24, 20, 280, 18);

	HpText = MakeLabel(WidgetTree, TEXT("HpText"));
	AddAt(HpText, 24, 40, 400, 22);
	MassText = MakeLabel(WidgetTree, TEXT("MassText"));
	AddAt(MassText, 24, 64, 520, 22);
	SlotText = MakeLabel(WidgetTree, TEXT("SlotText"));
	AddAt(SlotText, 24, 86, 400, 22);
	ActiveText = MakeLabel(WidgetTree, TEXT("ActiveText"));
	AddAt(ActiveText, 24, 108, 520, 22);
	HintText = MakeLabel(WidgetTree, TEXT("HintText"));
	AddAt(HintText, 24, 130, 620, 22);

	CrossH = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrossH"));
	CrossH->SetBrushColor(FLinearColor::White);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(CrossH);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(16.f, 2.f));
		S->SetPosition(FVector2D(0.f, 0.f));
	}
	CrossV = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrossV"));
	CrossV->SetBrushColor(FLinearColor::White);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(CrossV);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(2.f, 16.f));
		S->SetPosition(FVector2D(0.f, 0.f));
	}

	AimText = MakeLabel(WidgetTree, TEXT("AimText"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(AimText);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.f));
		S->SetPosition(FVector2D(0.f, 24.f));
		S->SetAutoSize(true);
	}

	ChannelBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ChannelBar"));
	ChannelBar->SetFillColorAndOpacity(FLinearColor(0.40f, 0.90f, 0.45f, 1.f));
	ChannelBar->SetVisibility(ESlateVisibility::Collapsed);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(ChannelBar);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.f));
		S->SetPosition(FVector2D(0.f, 48.f));
		S->SetSize(FVector2D(160.f, 10.f));
	}

	HintBottom = MakeLabel(WidgetTree, TEXT("HintBottom"), 14.f);
	HintBottom->SetText(FText::FromString(
		TEXT("WASD 이동  마우스 시점  좌클릭 공격  우클릭 형질  Q 형질 전환  몸에 닿으면 포식  Tab 형질  Esc 커서")));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(HintBottom);
		S->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		S->SetOffsets(FMargin(24.f, -58.f, 24.f, 36.f));
	}

	NestHint = MakeLabel(WidgetTree, TEXT("NestHint"));
	NestHint->SetVisibility(ESlateVisibility::Collapsed);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(NestHint);
		S->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		S->SetOffsets(FMargin(24.f, -82.f, 24.f, 60.f));
	}

	StatusText = MakeLabel(WidgetTree, TEXT("StatusText"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(StatusText);
		S->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		S->SetOffsets(FMargin(24.f, -112.f, 24.f, 90.f));
	}
}

void USlimeHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromWorld();
}

void USlimeHudWidget::RefreshFromWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AExpeditionDirector* Director = Cast<AExpeditionDirector>(
		UGameplayStatics::GetActorOfClass(World, AExpeditionDirector::StaticClass()));
	ASlimePawn* SoftSlime = nullptr;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		SoftSlime = Cast<ASlimePawn>(PC->GetPawn());
	}

	const float Hp = SoftSlime ? SoftSlime->GetHp() : 0.f;
	const float MaxHp = FMath::Max(1.f, SoftSlime ? SoftSlime->GetMaxHp() : 100.f);
	if (HpBar)
	{
		HpBar->SetPercent(Hp / MaxHp);
	}
	if (HpText)
	{
		HpText->SetText(FText::FromString(FString::Printf(TEXT("체력 %d"), FMath::CeilToInt(Hp))));
	}

	const float Settled = Director ? Director->GetSettledMass() : 0.f;
	const float Run = Director ? Director->GetRunMass() : 0.f;
	if (MassText)
	{
		MassText->SetText(FText::FromString(FString::Printf(TEXT("정착 질량 %.1f    이번 출정 %.1f"), Settled, Run)));
	}
	if (SlotText)
	{
		const int32 Slots = Director ? Director->GetConfirmedTraits().Num() : 0;
		const int32 Cap = Director ? Director->GetSlotCapacity() : 1;
		SlotText->SetText(FText::FromString(FString::Printf(TEXT("슬롯 %d/%d"), Slots, Cap)));
	}

	const ETraitId Active = Director ? Director->GetActiveTrait() : ETraitId::None;
	const bool bConfirmed = Director && Director->IsActiveConfirmed();
	if (ActiveText)
	{
		if (Active == ETraitId::None)
		{
			ActiveText->SetText(FText::FromString(TEXT("활성 형질 없음")));
		}
		else
		{
			ActiveText->SetText(FText::FromString(FString::Printf(
				TEXT("활성 %s (%s)"), *TraitText::Name(Active), bConfirmed ? TEXT("확정") : TEXT("불안정"))));
		}
	}
	if (HintText)
	{
		HintText->SetText(FText::FromString(TraitText::Hint(Active)));
	}

	const bool bEngulfing = SoftSlime && SoftSlime->GetEngulfTarget() != nullptr;
	const FLinearColor Cross = bEngulfing ? FLinearColor(0.45f, 0.95f, 0.40f) : FLinearColor::White;
	if (CrossH)
	{
		CrossH->SetBrushColor(Cross);
	}
	if (CrossV)
	{
		CrossV->SetBrushColor(Cross);
	}

	FString Aim;
	float Channel = 0.f;
	bool bShowChannel = false;
	if (SoftSlime && SoftSlime->GetEngulfTarget())
	{
		if (ADevourable* Prey = Cast<ADevourable>(SoftSlime->GetEngulfTarget()))
		{
			Aim = FString::Printf(TEXT("%s을(를) 몸으로 삼키는 중"), *Prey->GetLabel());
			if (Prey->Trait != ETraitId::None && Director)
			{
				Aim += FString::Printf(TEXT("    흡수 %d%%"), FMath::RoundToInt(Director->PreviewChance(Prey) * 100.f));
			}
			Channel = SoftSlime->GetChannel01();
			bShowChannel = true;
		}
	}
	else if (SoftSlime && SoftSlime->GetAimedPrey())
	{
		if (ADevourable* AimedPrey = Cast<ADevourable>(SoftSlime->GetAimedPrey()))
		{
			if (AimedPrey->Kind == EPreyKind::Creature && !AimedPrey->CanDevour())
			{
				const ACreatureAgent* Creature = Cast<ACreatureAgent>(AimedPrey);
				const int32 HpCeil = Creature ? FMath::CeilToInt(Creature->GetHp()) : 0;
				Aim = FString::Printf(TEXT("%s  체력 %d  쓰러뜨린 뒤 몸을 붙여 삼킨다"), *AimedPrey->GetLabel(), HpCeil);
			}
			else if (AimedPrey->CanDevour())
			{
				Aim = FString::Printf(TEXT("%s  몸을 붙여 삼킨다"), *AimedPrey->GetLabel());
				if (Director && AimedPrey->Trait != ETraitId::None)
				{
					Aim += FString::Printf(TEXT("  흡수 %d%%"), FMath::RoundToInt(Director->PreviewChance(AimedPrey) * 100.f));
				}
			}
		}
	}
	if (AimText)
	{
		AimText->SetText(FText::FromString(Aim));
	}
	if (ChannelBar)
	{
		ChannelBar->SetVisibility(bShowChannel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ChannelBar->SetPercent(Channel);
	}

	if (NestHint)
	{
		const bool bShowHint = Director && !bTraitPanelOpen;
		NestHint->SetVisibility(bShowHint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowHint)
		{
			NestHint->SetText(FText::FromString(TEXT("Tab: 형질 목록")));
		}
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Director ? Director->GetStatus() : FString()));
		StatusText->SetVisibility(bTraitPanelOpen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
