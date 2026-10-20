#include "ExpeditionDirector.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeGutContentComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AExpeditionDirector::AExpeditionDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	NestSpawnTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, RaisingSlimeUnits::NestSpawnZCm));
}

void AExpeditionDirector::BeginPlay()
{
	Super::BeginPlay();
	Status = TEXT("사물과 쓰러뜨린 개체를 삼켜라. Tab으로 형질을 본다.");
}

void AExpeditionDirector::RegisterSlime(ASlimePawn* InSlime)
{
	Slime = InSlime;
}

bool AExpeditionDirector::IsActiveConfirmed() const
{
	return Active != ETraitId::None && Confirmed.Contains(Active);
}

void AExpeditionDirector::CopyOwned(TArray<ETraitId>& OutOwned) const
{
	OutOwned.Reset();
	OutOwned.Append(Confirmed);
	for (ETraitId Trait : Unstable)
	{
		OutOwned.AddUnique(Trait);
	}
}

bool AExpeditionDirector::Owns(ETraitId Trait) const
{
	return Trait != ETraitId::None && (Confirmed.Contains(Trait) || Unstable.Contains(Trait));
}

void AExpeditionDirector::SyncActiveFromConfirmed()
{
	if (Confirmed.Num() > 0)
	{
		Active = Confirmed[0];
	}
	else if (!Unstable.Contains(Active))
	{
		Active = Unstable.Num() > 0 ? Unstable[0] : ETraitId::None;
	}
}

float AExpeditionDirector::PreviewChance(const ADevourable* Prey) const
{
	if (!Prey || Prey->Trait == ETraitId::None)
	{
		return 0.f;
	}

	const int32 Fails = Pity.FindRef(Prey->Trait);
	const float Compat = Confirmed.Contains(Prey->Trait) ? 1.2f : 1.f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Hunger = FMath::Clamp((Now - LastEatTime) / 20.f, 0.f, 1.f) * 0.1f;
	const float Chance = Prey->BaseChance * Compat * (1.f + Fails * 0.35f) + Hunger;
	return FMath::Clamp(Chance, 0.02f, 0.9f);
}

void AExpeditionDirector::CompleteDevour(ADevourable* Prey)
{
	if (!Prey)
	{
		return;
	}

	const bool bAlready = Owns(Prey->Trait);
	float Chance = 0.f;
	bool bSuccess = false;
	if (Prey->Trait != ETraitId::None && !bAlready)
	{
		Chance = PreviewChance(Prey);
		bSuccess = FMath::FRand() <= Chance;
	}

	LastEatTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// RunMass commits on Contained (after player fully covers prey) — not at channel end.

	if (Prey->Trait == ETraitId::None || bAlready)
	{
		Status = FString::Printf(TEXT("%s을(를) 겉에 붙였다. 안쪽으로 스며든다."), *Prey->GetLabel());
	}
	else if (bSuccess)
	{
		Unstable.Add(Prey->Trait);
		Active = Prey->Trait;
		Pity.Add(Prey->Trait, 0);
		Status = FString::Printf(
			TEXT("%s의 %s — 겉에 붙어 안쪽으로 (%d%%)."),
			*Prey->GetLabel(),
			*TraitText::Name(Prey->Trait),
			FMath::RoundToInt(Chance * 100.f));
	}
	else
	{
		const int32 Fails = Pity.FindRef(Prey->Trait);
		Pity.Add(Prey->Trait, Fails + 1);
		Status = FString::Printf(
			TEXT("%s을(를) 겉에 붙였다. %s 흡수 실패 (%d%%). 안쪽으로 스며든다."),
			*Prey->GetLabel(),
			*TraitText::Name(Prey->Trait),
			FMath::RoundToInt(Chance * 100.f));
	}

	if (Slime && Slime->GetGutContent())
	{
		Slime->GetGutContent()->Swallow(Prey);
	}
	else
	{
		Prey->BeginHeld();
		Prey->SetActorHiddenInGame(true);
	}
	if (Slime)
	{
		Slime->NotifyAte();
	}
}

void AExpeditionDirector::OnGutContained(float Mass, ETraitId /*Trait*/, ADevourable* Prey)
{
	RunMass += Mass;
	if (Slime)
	{
		Slime->RefreshBody();
	}
	Status = Prey
		? FString::Printf(TEXT("%s을(를) 삼켰다. 몸 안에서 소화 중…"), *Prey->GetLabel())
		: TEXT("삼킴 완료. 몸 안에서 소화 중…");
}

void AExpeditionDirector::OnGutAbsorbed(float Mass, ETraitId Trait, ADevourable* Prey)
{
	const float SettledNow = FMath::Clamp(Mass, 0.f, RunMass);
	SettledMass += SettledNow;
	RunMass = FMath::Max(0.f, RunMass - SettledNow);

	bool bPromoted = false;
	if (Trait != ETraitId::None && Unstable.Contains(Trait))
	{
		if (Confirmed.Contains(Trait))
		{
			Unstable.Remove(Trait);
			bPromoted = true;
		}
		else if (Confirmed.Num() < GetSlotCapacity())
		{
			Unstable.Remove(Trait);
			Confirmed.Add(Trait);
			bPromoted = true;
		}
		// Slot full: keep Unstable; mass still settled.
	}

	SyncActiveFromConfirmed();

	// Prey stays visible while dissolving; FinishGutAndRespawn starts cooldown after melt.

	if (Slime)
	{
		Slime->RefreshBody();
	}

	if (Trait != ETraitId::None && Unstable.Contains(Trait) && !bPromoted)
	{
		Status = FString::Printf(
			TEXT("질량 %.1f 정착. %s은(는) 슬롯이 가득 차 불안정으로 남았다."),
			SettledNow, *TraitText::Name(Trait));
	}
	else if (bPromoted)
	{
		Status = FString::Printf(
			TEXT("질량 %.1f 정착. %s 확정."), SettledNow, *TraitText::Name(Trait));
	}
	else
	{
		Status = FString::Printf(TEXT("질량 %.1f 정착했다."), SettledNow);
	}
}

void AExpeditionDirector::CycleActive()
{
	if (Confirmed.Num() == 0)
	{
		TArray<ETraitId> Owned;
		CopyOwned(Owned);
		if (Owned.Num() == 0)
		{
			Active = ETraitId::None;
			return;
		}
		const int32 Index = Owned.IndexOfByKey(Active);
		const int32 Next = (Index + 1) % Owned.Num();
		Active = Owned[Next >= 0 ? Next : 0];
	}
	else
	{
		const int32 Index = Confirmed.IndexOfByKey(Active);
		const int32 From = Index >= 0 ? Index : 0;
		const int32 Next = (From + 1) % Confirmed.Num();
		if (Next != 0)
		{
			// Rotate so chosen trait becomes top (Active).
			ETraitId Chosen = Confirmed[Next];
			Confirmed.RemoveAt(Next);
			Confirmed.Insert(Chosen, 0);
		}
		Active = Confirmed[0];
	}

	const FString State = IsActiveConfirmed() ? TEXT("확정") : TEXT("불안정");
	Status = FString::Printf(TEXT("활성 형질: %s (%s)"), *TraitText::Name(Active), *State);
}

bool AExpeditionDirector::TryConfirm(ETraitId Trait)
{
	if (!Unstable.Contains(Trait))
	{
		return false;
	}

	if (Confirmed.Contains(Trait))
	{
		Unstable.Remove(Trait);
		Status = FString::Printf(TEXT("%s은(는) 이미 확정되어 있다."), *TraitText::Name(Trait));
		SyncActiveFromConfirmed();
		return true;
	}

	if (Confirmed.Num() >= GetSlotCapacity())
	{
		Status = TEXT("슬롯이 가득 찼다. 흡수만 되고 형질은 불안정으로 남는다.");
		return false;
	}

	Unstable.Remove(Trait);
	Confirmed.Add(Trait);
	SyncActiveFromConfirmed();
	Status = FString::Printf(TEXT("%s 확정. 이후에도 남는다."), *TraitText::Name(Trait));
	return true;
}

void AExpeditionDirector::Spit(ETraitId Trait)
{
	if (Confirmed.Remove(Trait) <= 0)
	{
		return;
	}

	SyncActiveFromConfirmed();
	Status = FString::Printf(TEXT("%s을(를) 토해냈다."), *TraitText::Name(Trait));
	if (Slime)
	{
		Slime->PlaySpitSplash();
	}
}

bool AExpeditionDirector::PurgeConfirmedTrait(ETraitId Trait)
{
	if (!bHasTraitPurgeItem || !Confirmed.Contains(Trait))
	{
		return false;
	}

	Confirmed.Remove(Trait);
	SyncActiveFromConfirmed();
	Status = FString::Printf(TEXT("%s을(를) 삭제했다."), *TraitText::Name(Trait));
	return true;
}

bool AExpeditionDirector::ReorderConfirmed(ETraitId Dragged, ETraitId DropTarget)
{
	const int32 From = Confirmed.IndexOfByKey(Dragged);
	const int32 To = Confirmed.IndexOfByKey(DropTarget);
	if (From < 0 || To < 0 || From == To)
	{
		return false;
	}

	Confirmed.RemoveAt(From);
	const int32 InsertAt = Confirmed.IndexOfByKey(DropTarget);
	if (InsertAt < 0)
	{
		Confirmed.Add(Dragged);
	}
	else if (From < To)
	{
		Confirmed.Insert(Dragged, InsertAt + 1);
	}
	else
	{
		Confirmed.Insert(Dragged, InsertAt);
	}

	SyncActiveFromConfirmed();
	Status = FString::Printf(TEXT("활성 형질: %s"), *TraitText::Name(Active));
	return true;
}

void AExpeditionDirector::Settle()
{
	// Legacy nest settle path — field absorb replaces this for normal play.
	// Still settles all run mass without nest teleport / global prey reset.
	const int32 Discarded = Unstable.Num();
	SettledMass += RunMass;
	RunMass = 0.f;
	Unstable.Reset();
	SyncActiveFromConfirmed();

	if (Slime)
	{
		Slime->BeginGutDissolve();
		Slime->Recover();
	}

	Status = Discarded > 0
		? FString::Printf(TEXT("질량을 정착했다. 불안정 형질 %d개는 남지 않았다."), Discarded)
		: TEXT("질량을 정착했다.");
}

void AExpeditionDirector::Die()
{
	if (Slime)
	{
		Slime->PlayDeathSplash();
		Slime->ClearGutContent();
	}

	RunMass = 0.f;
	Unstable.Reset();
	SyncActiveFromConfirmed();

	// Unabsorbed prey never got absorb-complete respawn — kick cooldowns now.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADevourable::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		if (ADevourable* Prey = Cast<ADevourable>(Actor))
		{
			if (Prey->IsConsumed())
			{
				Prey->BeginRespawnCooldown();
			}
		}
	}

	if (Slime)
	{
		Slime->Recover();
	}
	TeleportSlimeToPlayerStart();

	Status = TEXT("몸이 풀렸다. 이번 출정의 질량과 불안정 형질은 사라졌다.");
}

void AExpeditionDirector::TeleportSlimeToPlayerStart()
{
	if (!Slime || !GetWorld())
	{
		return;
	}

	FTransform Spawn = NestSpawnTransform;
	if (AActor* Start = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass()))
	{
		Spawn = Start->GetActorTransform();
	}

	Slime->TeleportToSpawn(Spawn);
}

void AExpeditionDirector::ResetWorld()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADevourable::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		if (ADevourable* Prey = Cast<ADevourable>(Actor))
		{
			Prey->RestoreScenePlacement();
		}
	}
}
