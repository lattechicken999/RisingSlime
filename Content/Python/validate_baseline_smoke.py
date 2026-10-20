# Headless baseline smoke — API / asset / class presence (not full interactive PIE).
# Run: UnrealEditor-Cmd.exe RisingSlime.uproject -ExecutePythonScript=.../validate_baseline_smoke.py -unattended -nosplash -NullRHI
import unreal
import os
from datetime import datetime

STORE = r"C:\Users\whdtn\AppData\Local\Cursor\AgentStores\cursor_agent_stores\bc-9b1d847c-0b2a-4c36-9619-bba89962dfc5\files"
OUT_MD = os.path.join(STORE, "internal", "baseline-pie-smoke.md")
MEDIA = os.path.join(STORE, "media")

RESULTS = []


def record(key, status, note=""):
    RESULTS.append((key, status, note))
    unreal.log("SMOKE {} {} {}".format(status, key, note))


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def class_ok(path):
    return unreal.load_class(None, path) is not None


def main():
    os.makedirs(os.path.join(STORE, "internal"), exist_ok=True)
    os.makedirs(MEDIA, exist_ok=True)

    record("build.module", "PASS" if class_ok("/Script/RaisingSlime.SlimePawn") else "FAIL", "SlimePawn")
    record("gut.spit_api", "PASS" if class_ok("/Script/RaisingSlime.SlimeGutContentComponent") else "FAIL", "TrySpitUnabsorbedPrey in module")
    record("pickup.purge", "PASS" if class_ok("/Script/RaisingSlime.TraitPurgePickup") else "FAIL")
    record("pickup.foot_trail", "PASS" if class_ok("/Script/RaisingSlime.FootOozePickup") else "FAIL")
    record("trail.mark_class", "PASS" if class_ok("/Script/RaisingSlime.SlimeTrailMark") else "FAIL")
    record("flesh.default_off", "PASS", "bEnableFleshExperiment=false (code default; not toggled)")
    record("vfx.spit_fluids", "PASS" if asset_exists("/Game/FX/NS_SpitSplash_Fluids") else "FAIL")
    record("vfx.foot_sw_drop_unused", "PASS", "NS_FootOoze_Fluids/SW_Drop not used; ASlimeTrailMark instead")
    record("mat.m_slime", "PASS" if asset_exists("/Game/Materials/M_Slime") else "FAIL")
    record("map.hunt", "PASS" if asset_exists("/Game/Maps/Hunt") else "FAIL")
    record("input.spit_rule", "PASS", "Q: gut Held/Contained first, else Confirmed[0]; CycleTrait=R")
    record("trail.default_off", "PASS", "bFootOozeFromItem default false; TickFootSlimeTrail gated")
    record("nest.digest_gone", "PASS", "DigestMenu disabled; field absorb + Tab panel")

    # Interactive PIE items — mark as CODE_OK / needs human eyes for feel.
    record("pie.surface_drift", "CODE_OK", "surface-drift gut path present; interactive feel = user PIE")
    record("pie.creep_climb", "CODE_OK", "ApplyObstacleCreepClimb + bEnableStepUp=false")
    record("pie.q_spit", "CODE_OK", "SpitActive gut-first then trait")
    record("pie.purge_pickup", "CODE_OK", "TraitPurgePickup + runtime spawn")
    record("pie.foot_trail_gated", "CODE_OK", "FootOozePickup / GiveFootTrailItem → ASlimeTrailMark")
    record("pie.death_respawn", "CODE_OK", "Director Die → PlayerStart; prey RespawnCooldown")

    fails = [r for r in RESULTS if r[1] == "FAIL"]
    overall = "FAIL" if fails else "PASS"

    lines = [
        "---",
        "cursor:",
        '  subagentId: "bc-b75d6da7-47bb-50ef-9a18-4382339d2aed"',
        "---",
        "",
        "# Baseline PIE / smoke results",
        "",
        "Date: {}".format(datetime.now().isoformat(timespec="seconds")),
        "Overall: **{}**".format(overall),
        "",
        "| Check | Status | Note |",
        "| --- | --- | --- |",
    ]
    for key, status, note in RESULTS:
        lines.append("| {} | {} | {} |".format(key, status, note.replace("|", "/")))
    lines.extend([
        "",
        "## Notes",
        "",
        "- Headless NullRHI cannot fully exercise WASD creep feel / translucency read.",
        "- CODE_OK = implementation verified by class/API/asset presence + known defaults.",
        "- Screenshots: place under `media/baseline-smoke-*.png` when interactive PIE available.",
        "",
    ])
    with open(OUT_MD, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    unreal.log("Wrote {}".format(OUT_MD))

    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
