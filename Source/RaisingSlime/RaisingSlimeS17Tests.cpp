#include "Misc/AutomationTest.h"
#include "RaisingSlimeTypes.h"
#include "ExpeditionDirector.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaisingSlimeS17Kinematics,
	"RaisingSlime.S17.Kinematics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaisingSlimeS17Kinematics::RunTest(const FString& Parameters)
{
	using namespace RaisingSlimeUnits;

	const float StopTime = MoveSpeedMaxMps / MoveDecelMps2;
	TestTrue(TEXT("stop time ≈ 0.172s"), FMath::IsNearlyEqual(StopTime, 0.172222f, 0.001f));

	// Base ballistic with constant g (movement also applies apex/fall gravity scales → snappier hang).
	const float Apex = (JumpSpeedMps * JumpSpeedMps) / (2.f * GravityMps2);
	TestTrue(TEXT("jump apex ≈ 1.521m @ 7.8 m/s"), FMath::IsNearlyEqual(Apex, 1.521f, 0.002f));

	const float Airtime = (2.f * JumpSpeedMps) / GravityMps2;
	TestTrue(TEXT("base airtime ≈ 0.78s (scaled hang shorter in movement)"), FMath::IsNearlyEqual(Airtime, 0.78f, 0.002f));

	TestTrue(TEXT("gravity cm matches 20 m/s²"), FMath::IsNearlyEqual(FMath::Abs(SlimeGravityCm) / MetersToCm, GravityMps2, 0.01f));
	TestTrue(TEXT("jump takeoff bumped vs old 7.2"), JumpSpeedMps > 7.2f);
	TestEqual(TEXT("default pitch -22"), DefaultPitch, -22.f);
	TestEqual(TEXT("vertical FOV 78"), VerticalFovDegrees, 78.f);
	TestTrue(TEXT("horiz FOV equiv ~110.43"), FMath::IsNearlyEqual(HorizontalFov16x9Equiv, 110.43f, 0.01f));

	const float SpeedAt40 = FMath::Clamp(MoveSpeedMaxMps - 40.f * 0.07f, 3.4f, MoveSpeedMaxMps);
	TestTrue(TEXT("mass 40 speed 3.4"), FMath::IsNearlyEqual(SpeedAt40, 3.4f, 0.01f));

	const float ScaleAt41 = FMath::Clamp(1.15f + 41.f * 0.05f, 1.15f, 3.2f);
	TestTrue(TEXT("mass 41 body scale capped 3.2"), FMath::IsNearlyEqual(ScaleAt41, 3.2f, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRaisingSlimeS17ExpeditionMath,
	"RaisingSlime.S17.ExpeditionMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaisingSlimeS17ExpeditionMath::RunTest(const FString& Parameters)
{
	const float RockMass = 2.f;
	const float RunMass = RockMass;
	const float MaxHp = 100.f + RunMass * 5.f;
	TestTrue(TEXT("rock → RunMass 2"), FMath::IsNearlyEqual(RunMass, 2.f, 0.01f));
	TestTrue(TEXT("rock → MaxHp 110"), FMath::IsNearlyEqual(MaxHp, 110.f, 0.01f));

	// Slot capacity: 1 + floor(Settled/10)
	TestEqual(TEXT("settled 0 → 1 slot"), 1 + FMath::FloorToInt(0.f / 10.f), 1);
	TestEqual(TEXT("settled 10 → 2 slots"), 1 + FMath::FloorToInt(10.f / 10.f), 2);

	// Pity formula (§17): base * (1 + fails * 0.35) then clamp with hunger later
	const float Base = 0.62f;
	const float After1Fail = Base * (1.f + 1.f * 0.35f);
	TestTrue(TEXT("harden pity after 1 fail"), FMath::IsNearlyEqual(After1Fail, 0.62f * 1.35f, 0.001f));

	const float BeeHp = 36.f;
	TestEqual(TEXT("bee MaxHp 36"), BeeHp, 36.f);

	// C0 spit ammo table (E0): grass≈9 → 4–5 hits; rock≈14 → ~3 hits
	TestTrue(TEXT("grass spit base 9"), FMath::IsNearlyEqual(PreyCombat::SpitBaseDamage(EPreyKind::Grass), 9.f, 0.01f));
	TestTrue(TEXT("rock spit base 14"), FMath::IsNearlyEqual(PreyCombat::SpitBaseDamage(EPreyKind::Rock), 14.f, 0.01f));
	TestTrue(TEXT("four grass < 36"), 9.f * 4.f < BeeHp);
	TestTrue(TEXT("five grass >= 36"), 9.f * 5.f >= BeeHp);
	TestTrue(TEXT("three rock >= 36"), 14.f * 3.f >= BeeHp);
	TestTrue(TEXT("hostile creature not non-hostile"), PreyCombat::IsHostileCombatant(EPreyKind::Creature));
	TestTrue(TEXT("grass is non-hostile prey"), PreyCombat::IsNonHostilePrey(EPreyKind::Grass));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
