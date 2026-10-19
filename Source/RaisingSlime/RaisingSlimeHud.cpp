#include "RaisingSlimeHud.h"
#include "ExpeditionDirector.h"
#include "Devourable.h"
#include "CreatureAgent.h"
#include "NestZone.h"
#include "RaisingSlimePlayerController.h"
#include "RaisingSlimeTypes.h"
#include "SlimePawn.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

ARaisingSlimeHUD::ARaisingSlimeHUD()
{
}

void ARaisingSlimeHUD::OpenDigestMenu()
{
	bDigestOpen = true;
	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		PC->SetMenuOpen(true);
	}
}

void ARaisingSlimeHUD::CloseDigestMenu()
{
	bDigestOpen = false;
	if (ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner))
	{
		PC->SetMenuOpen(false);
		PC->SetCursorUnlocked(false);
	}
}

void ARaisingSlimeHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	AExpeditionDirector* Director = Cast<AExpeditionDirector>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AExpeditionDirector::StaticClass()));
	ASlimePawn* SoftSlime = Cast<ASlimePawn>(GetOwningPawn());
	ARaisingSlimePlayerController* PC = Cast<ARaisingSlimePlayerController>(PlayerOwner);

	DrawStatusBars(Director, SoftSlime);

	if (bDigestOpen && Director && PC)
	{
		DrawDigestPanel(Director, PC);
	}
}

void ARaisingSlimeHUD::DrawStatusBars(AExpeditionDirector* Director, ASlimePawn* SoftSlime)
{
	const float SX = Canvas->SizeX / 1920.f;
	const float SY = Canvas->SizeY / 1080.f;
	auto PX = [SX](float X) { return X * SX; };
	auto PY = [SY](float Y) { return Y * SY; };

	const float Hp = SoftSlime ? SoftSlime->GetHp() : 0.f;
	const float MaxHp = FMath::Max(1.f, SoftSlime ? SoftSlime->GetMaxHp() : 100.f);
	const float Settled = Director ? Director->GetSettledMass() : 0.f;
	const float Run = Director ? Director->GetRunMass() : 0.f;
	const int32 Slots = Director ? Director->GetConfirmedTraits().Num() : 0;
	const int32 Cap = Director ? Director->GetSlotCapacity() : 1;
	const ETraitId Active = Director ? Director->GetActiveTrait() : ETraitId::None;
	const bool bConfirmed = Director && Director->IsActiveConfirmed();

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), PX(24), PY(20), PX(280), PY(18));
	DrawRect(FLinearColor(0.75f, 0.18f, 0.20f, 1.f), PX(24), PY(20), PX(280) * (Hp / MaxHp), PY(18));

	DrawText(FString::Printf(TEXT("체력 %d"), FMath::CeilToInt(Hp)), FLinearColor::White, PX(24), PY(40));
	DrawText(FString::Printf(TEXT("정착 질량 %.1f    이번 출정 %.1f"), Settled, Run), FLinearColor::White, PX(24), PY(64));
	DrawText(FString::Printf(TEXT("슬롯 %d/%d"), Slots, Cap), FLinearColor::White, PX(24), PY(86));

	if (Active == ETraitId::None)
	{
		DrawText(TEXT("활성 형질 없음"), FLinearColor::White, PX(24), PY(108));
	}
	else
	{
		DrawText(
			FString::Printf(TEXT("활성 %s (%s)"), *TraitText::Name(Active), bConfirmed ? TEXT("확정") : TEXT("불안정")),
			FLinearColor::White, PX(24), PY(108));
		DrawText(TraitText::Hint(Active), FLinearColor::White, PX(24), PY(130));
	}

	const bool bEngulfing = SoftSlime && SoftSlime->GetEngulfTarget() != nullptr;
	const FLinearColor Cross = bEngulfing ? FLinearColor(0.45f, 0.95f, 0.40f) : FLinearColor::White;
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	DrawRect(Cross, CX - PX(8), CY - PY(1), PX(16), PY(2));
	DrawRect(Cross, CX - PX(1), CY - PY(8), PX(2), PY(16));

	if (SoftSlime && SoftSlime->GetEngulfTarget())
	{
		if (ADevourable* Prey = Cast<ADevourable>(SoftSlime->GetEngulfTarget()))
		{
			FString Msg = FString::Printf(TEXT("%s을(를) 몸으로 삼키는 중"), *Prey->GetLabel());
			if (Prey->Trait != ETraitId::None && Director)
			{
				Msg += FString::Printf(TEXT("    흡수 %d%%"), FMath::RoundToInt(Director->PreviewChance(Prey) * 100.f));
			}
			DrawText(Msg, FLinearColor::White, CX - PX(160), CY + PY(24));
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), CX - PX(80), CY + PY(48), PX(160), PY(10));
			DrawRect(FLinearColor(0.40f, 0.90f, 0.45f, 1.f), CX - PX(80), CY + PY(48), PX(160) * SoftSlime->GetChannel01(), PY(10));
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
				DrawText(
					FString::Printf(TEXT("%s  체력 %d  쓰러뜨린 뒤 몸을 붙여 삼킨다"), *AimedPrey->GetLabel(), HpCeil),
					FLinearColor::White, CX - PX(200), CY + PY(24));
			}
			else if (AimedPrey->CanDevour())
			{
				DrawText(
					FString::Printf(TEXT("%s  몸을 붙여 삼킨다"), *AimedPrey->GetLabel()),
					FLinearColor::White, CX - PX(160), CY + PY(24));
				if (Director && AimedPrey->Trait != ETraitId::None)
				{
					DrawText(
						FString::Printf(TEXT("흡수 %d%%"), FMath::RoundToInt(Director->PreviewChance(AimedPrey) * 100.f)),
						FLinearColor::White, CX - PX(40), CY + PY(44));
				}
			}
		}
	}

	DrawText(
		TEXT("WASD 이동  마우스 시점  좌클릭 공격  우클릭 형질  Q 형질 전환  몸에 닿으면 포식  E 소화실  Esc 커서"),
		FLinearColor::White, PX(24), Canvas->SizeY - PY(58));

	if (Director && !bDigestOpen)
	{
		ANestZone* Nest = Cast<ANestZone>(UGameplayStatics::GetActorOfClass(GetWorld(), ANestZone::StaticClass()));
		if (Nest && Nest->IsPlayerInside())
		{
			DrawText(TEXT("둥지 안이다. E로 소화실을 연다."), FLinearColor::White, PX(24), Canvas->SizeY - PY(82));
		}
		DrawText(Director->GetStatus(), FLinearColor::White, PX(24), Canvas->SizeY - PY(112));
	}
}

void ARaisingSlimeHUD::DrawDigestPanel(AExpeditionDirector* Director, ARaisingSlimePlayerController* PC)
{
	const float SX = Canvas->SizeX / 1920.f;
	const float SY = Canvas->SizeY / 1080.f;
	const float W = 460.f * SX;
	const float H = 380.f * SY;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;

	DrawRect(FLinearColor(0.05f, 0.08f, 0.10f, 0.92f), X, Y, W, H);
	DrawText(TEXT("소화실"), FLinearColor::White, X + 16.f * SX, Y + 12.f * SY);
	DrawText(
		FString::Printf(TEXT("이번 출정 질량 %.1f  /  정착 %.1f"), Director->GetRunMass(), Director->GetSettledMass()),
		FLinearColor::White, X + 16.f * SX, Y + 48.f * SY);
	DrawText(
		FString::Printf(TEXT("확정 슬롯 %d/%d"), Director->GetConfirmedTraits().Num(), Director->GetSlotCapacity()),
		FLinearColor::White, X + 16.f * SX, Y + 72.f * SY);

	float RowY = Y + 110.f * SY;
	DrawText(TEXT("불안정"), FLinearColor(0.9f, 0.85f, 0.4f), X + 16.f * SX, RowY);
	RowY += 24.f * SY;
	if (Director->GetUnstableTraits().Num() == 0)
	{
		DrawText(TEXT("없음"), FLinearColor::Gray, X + 24.f * SX, RowY);
		RowY += 22.f * SY;
	}
	else
	{
		for (ETraitId Trait : Director->GetUnstableTraits())
		{
			DrawText(TraitText::Name(Trait), FLinearColor::White, X + 24.f * SX, RowY);
			RowY += 22.f * SY;
		}
	}

	DrawText(TEXT("확정"), FLinearColor(0.5f, 0.85f, 1.f), X + 16.f * SX, RowY);
	RowY += 24.f * SY;
	if (Director->GetConfirmedTraits().Num() == 0)
	{
		DrawText(TEXT("없음"), FLinearColor::Gray, X + 24.f * SX, RowY);
		RowY += 22.f * SY;
	}
	else
	{
		for (ETraitId Trait : Director->GetConfirmedTraits())
		{
			DrawText(TraitText::Name(Trait), FLinearColor::White, X + 24.f * SX, RowY);
			RowY += 22.f * SY;
		}
	}

	DrawText(
		TEXT("[1] 첫 불안정 확정  [2] 첫 확정 토해내기  [3] 질량 정착  [Esc] 닫기"),
		FLinearColor::White, X + 16.f * SX, Y + H - 48.f * SY);

	if (PC && PC->WasInputKeyJustPressed(EKeys::One) && Director->GetUnstableTraits().Num() > 0)
	{
		Director->TryConfirm(Director->GetUnstableTraits()[0]);
	}
	if (PC && PC->WasInputKeyJustPressed(EKeys::Two) && Director->GetConfirmedTraits().Num() > 0)
	{
		Director->Spit(Director->GetConfirmedTraits()[0]);
	}
	if (PC && PC->WasInputKeyJustPressed(EKeys::Three))
	{
		Director->Settle();
		CloseDigestMenu();
	}
	if (PC && PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		CloseDigestMenu();
	}

	(void)DigestHotIndex;
}
