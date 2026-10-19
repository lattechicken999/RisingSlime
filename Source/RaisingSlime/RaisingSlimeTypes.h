#pragma once

#include "CoreMinimal.h"
#include "RaisingSlimeTypes.generated.h"

UENUM(BlueprintType)
enum class ETraitId : uint8
{
	None UMETA(DisplayName = "없음"),
	Harden UMETA(DisplayName = "경화"),
	Bounce UMETA(DisplayName = "탄성"),
	Poison UMETA(DisplayName = "독")
};

UENUM(BlueprintType)
enum class EPreyKind : uint8
{
	Object UMETA(DisplayName = "사물"),
	Creature UMETA(DisplayName = "개체")
};

namespace TraitText
{
	inline FString Name(ETraitId Id)
	{
		switch (Id)
		{
		case ETraitId::Harden: return TEXT("경화");
		case ETraitId::Bounce: return TEXT("탄성");
		case ETraitId::Poison: return TEXT("독");
		default: return TEXT("없음");
		}
	}

	inline FString Hint(ETraitId Id)
	{
		switch (Id)
		{
		case ETraitId::Harden: return TEXT("우클릭으로 잠시 피해가 줄어든다");
		case ETraitId::Bounce: return TEXT("공중 추가 점프, 우클릭으로 미끄러지듯 전진");
		case ETraitId::Poison: return TEXT("좌클릭 공격에 독이 실린다");
		default: return FString();
		}
	}
}

namespace RaisingSlimeUnits
{
	constexpr float MetersToCm = 100.f;
	constexpr float FixedStep = 0.02f;
	constexpr int32 MaxSubsteps = 5;
	constexpr float SlimeGravityCm = -2000.f;
	constexpr float CreatureGravityCm = -981.f;
	constexpr float LookDegreesPerPixel = 0.22f;
	constexpr float DefaultPitch = -22.f;
	constexpr float PitchMin = -70.f;
	constexpr float PitchMax = 50.f;
	constexpr float BodyHeightCm = 55.f;
	constexpr float NestSpawnZCm = 16.f;
}
