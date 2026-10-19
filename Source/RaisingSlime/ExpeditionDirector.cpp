#include "ExpeditionDirector.h"
#include "Devourable.h"
#include "SlimePawn.h"
#include "SlimeGutContentComponent.h"
#include "Kismet/GameplayStatics.h"

AExpeditionDirector::AExpeditionDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	NestSpawnTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, RaisingSlimeUnits::NestSpawnZCm));
}

void AExpeditionDirector::BeginPlay()
{
	Super::BeginPlay();
	Status = TEXT("둥지에서 나가 사물과 쓰러뜨린 개체를 삼켜라.");
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

	RunMass += Prey->Mass;
	LastEatTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (Prey->Trait == ETraitId::None || bAlready)
	{
		Status = FString::Printf(TEXT("%s을(를) 삼켰다. 질량 +%.1f. 새 형질은 없다."), *Prey->GetLabel(), Prey->Mass);
	}
	else if (bSuccess)
	{
		Unstable.Add(Prey->Trait);
		Active = Prey->Trait;
		Pity.Add(Prey->Trait, 0);
		Status = FString::Printf(
			TEXT("%s의 %s 흡수 성공 (%d%%). 귀환 전에는 불안정하다."),
			*Prey->GetLabel(),
			*TraitText::Name(Prey->Trait),
			FMath::RoundToInt(Chance * 100.f));
	}
	else
	{
		const int32 Fails = Pity.FindRef(Prey->Trait);
		Pity.Add(Prey->Trait, Fails + 1);
		Status = FString::Printf(
			TEXT("%s을(를) 삼켰다. %s 흡수 실패 (%d%%). 질량만 늘었다."),
			*Prey->GetLabel(),
			*TraitText::Name(Prey->Trait),
			FMath::RoundToInt(Chance * 100.f));
	}

	// Gut proxy first (while prey mesh still valid), then hide world prey.
	if (Slime && Slime->GetGutContent())
	{
		Slime->GetGutContent()->Swallow(Prey);
	}
	Prey->Consume();
	if (Slime)
	{
		Slime->NotifyAte();
		Slime->RefreshBody();
	}
}

void AExpeditionDirector::CycleActive()
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
		return true;
	}

	if (Confirmed.Num() >= GetSlotCapacity())
	{
		Status = TEXT("슬롯이 가득 찼다. 하나를 토해내야 새 형질을 확정할 수 있다.");
		return false;
	}

	Unstable.Remove(Trait);
	Confirmed.Add(Trait);
	Active = Trait;
	Status = FString::Printf(TEXT("%s 확정. 이후 출정에도 남는다."), *TraitText::Name(Trait));
	return true;
}

void AExpeditionDirector::Spit(ETraitId Trait)
{
	if (Confirmed.Remove(Trait) <= 0)
	{
		return;
	}

	if (Active == Trait)
	{
		TArray<ETraitId> Owned;
		CopyOwned(Owned);
		Active = Owned.Num() > 0 ? Owned[0] : ETraitId::None;
	}

	Status = FString::Printf(TEXT("%s을(를) 토해냈다."), *TraitText::Name(Trait));
	if (Slime)
	{
		Slime->PlaySpitSplash();
	}
}

void AExpeditionDirector::Settle()
{
	const int32 Discarded = Unstable.Num();
	SettledMass += RunMass;
	RunMass = 0.f;
	Unstable.Reset();
	if (!Confirmed.Contains(Active))
	{
		TArray<ETraitId> Owned;
		CopyOwned(Owned);
		Active = Owned.Num() > 0 ? Owned[0] : ETraitId::None;
	}

	// Digestion complete: melt gut contents, then restore world prey / nest.
	if (Slime)
	{
		Slime->BeginGutDissolve();
	}
	ResetWorld();
	if (Slime)
	{
		Slime->Recover();
		Slime->TeleportToNest(NestSpawnTransform);
	}

	Status = Discarded > 0
		? FString::Printf(TEXT("질량을 정착했다. 확정하지 않은 형질 %d개는 몸에 남지 않았다."), Discarded)
		: TEXT("질량을 정착했다. 다음 출정을 나갈 수 있다.");
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
	if (!Confirmed.Contains(Active))
	{
		TArray<ETraitId> Owned;
		CopyOwned(Owned);
		Active = Owned.Num() > 0 ? Owned[0] : ETraitId::None;
	}

	ResetWorld();
	if (Slime)
	{
		Slime->Recover();
		Slime->TeleportToNest(NestSpawnTransform);
	}

	Status = TEXT("몸이 풀렸다. 이번 출정의 질량과 불안정 형질은 사라졌다.");
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
