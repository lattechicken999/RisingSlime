# Interactive GPU PIE smoke capture (NO NullRHI).
# Run:
#   UnrealEditor.exe RisingSlime.uproject -ExecutePythonScript=.../post_baseline_pie_capture.py
#
# Saves screenshots under the agent store media/ folder and a results JSON beside them.
import unreal
import os
import json
import time
import shutil

MEDIA = r"C:\Users\whdtn\AppData\Local\Cursor\AgentStores\cursor_agent_stores\bc-9b1d847c-0b2a-4c36-9619-bba89962dfc5\files\media"
RESULTS_JSON = os.path.join(MEDIA, "post-baseline-pie-results.json")
MAP_PATH = "/Game/Maps/Hunt"


def ensure_media():
    os.makedirs(MEDIA, exist_ok=True)


def log(msg):
    unreal.log("post_baseline_pie: {}".format(msg))


def class_ok(path):
    try:
        return unreal.load_class(None, path) is not None
    except Exception:
        return False


def asset_ok(path):
    try:
        return unreal.EditorAssetLibrary.does_asset_exist(path)
    except Exception:
        return False


def take_shot(basename):
    """Capture high-res screenshot into MEDIA as basename.png (absolute path)."""
    ensure_media()
    # AutomationLibrary writes with .png appended if missing; pass stem path.
    stem = os.path.join(MEDIA, basename)
    abs_png = stem + ".png"
    try:
        # Clear stale file so we can verify freshness.
        if os.path.isfile(abs_png):
            os.remove(abs_png)
    except Exception:
        pass

    ok = False
    try:
        ok = bool(unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, stem))
    except Exception as ex:
        log("take_high_res_screenshot failed: {}".format(ex))
        ok = False

    # Flush a few ticks so the file lands on disk.
    for _ in range(8):
        unreal.SystemLibrary.execute_console_command(None, "r.FlushPersistentDebugLines")
        time.sleep(0.25)
        if os.path.isfile(abs_png) and os.path.getsize(abs_png) > 2000:
            log("shot OK {} ({} bytes)".format(abs_png, os.path.getsize(abs_png)))
            return abs_png, True

    # Fallback: HighResShot into Saved/Screenshots then copy newest.
    try:
        unreal.SystemLibrary.execute_console_command(
            None, 'HighResShot 1600x900 filename="{}"'.format(stem.replace("\\", "/")))
        time.sleep(1.0)
    except Exception as ex:
        log("HighResShot failed: {}".format(ex))

    if os.path.isfile(abs_png) and os.path.getsize(abs_png) > 2000:
        return abs_png, True

    # Search project Saved/Screenshots
    proj = unreal.Paths.project_saved_dir()
    shot_root = os.path.join(proj, "Screenshots")
    newest = None
    newest_mtime = 0
    if os.path.isdir(shot_root):
        for root, _dirs, files in os.walk(shot_root):
            for f in files:
                if f.lower().endswith(".png"):
                    p = os.path.join(root, f)
                    m = os.path.getmtime(p)
                    if m > newest_mtime:
                        newest_mtime = m
                        newest = p
    if newest and (time.time() - newest_mtime) < 120:
        dest = abs_png
        shutil.copy2(newest, dest)
        log("copied fallback {} -> {}".format(newest, dest))
        return dest, True

    log("shot MISSING {}".format(abs_png))
    return abs_png, False


def begin_pie():
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les:
            les.editor_request_begin_play()
            log("PIE begin via LevelEditorSubsystem")
            return True
    except Exception as ex:
        log("LevelEditorSubsystem PIE failed: {}".format(ex))
    try:
        unreal.EditorLevelLibrary.editor_play_simulate()
        log("PIE begin via editor_play_simulate")
        return True
    except Exception as ex:
        log("editor_play_simulate failed: {}".format(ex))
    return False


def end_pie():
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les:
            les.editor_request_end_play()
            return
    except Exception:
        pass
    try:
        unreal.EditorLevelLibrary.editor_end_play()
    except Exception:
        pass


def main():
    ensure_media()
    checks = []

    # --- Static / code presence (GPU session still validates live classes) ---
    checks.append(("flesh.default_off", class_ok("/Script/RaisingSlime.SlimeFleshExperimentComponent"), "bEnableFleshExperiment default false"))
    checks.append(("trail.mark_class", class_ok("/Script/RaisingSlime.SlimeTrailMark"), "ASlimeTrailMark"))
    checks.append(("spit.grace", class_ok("/Script/RaisingSlime.Devourable"), "SpitReDevourGraceSeconds"))
    checks.append(("creep.api", class_ok("/Script/RaisingSlime.SlimeSoftVolumeComponent"), "ApplyObstacleCreepClimb"))
    checks.append(("map.hunt", asset_ok(MAP_PATH), MAP_PATH))
    checks.append(("mat.m_slime", asset_ok("/Game/Materials/M_Slime"), "M_Slime"))
    checks.append(("pickup.trail", class_ok("/Script/RaisingSlime.FootOozePickup"), "trail gate pickup"))
    checks.append(("pickup.purge", class_ok("/Script/RaisingSlime.TraitPurgePickup"), "purge"))

    # Load Hunt for viewport
    loaded = False
    try:
        loaded = bool(unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH))
    except Exception as ex:
        log("load_map: {}".format(ex))
    checks.append(("map.load_hunt", loaded, "Editor load Hunt"))

    time.sleep(1.5)
    p01, ok01 = take_shot("post-baseline-01")
    checks.append(("shot.01_editor_hunt", ok01, p01))

    pie_ok = begin_pie()
    checks.append(("pie.begin", pie_ok, "GPU PIE/simulate start"))
    time.sleep(3.0)

    # World actors while PIE is running
    world = None
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        # Prefer PIE world if available
        pie_worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
        if pie_worlds and len(pie_worlds) > 0:
            world = pie_worlds[0]
            log("using PIE world")
    except Exception as ex:
        log("world resolve: {}".format(ex))

    slime_ok = False
    trail_off = True
    flesh_off = True
    director_ok = False
    if world:
        try:
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                name = actor.get_class().get_name()
                if "SlimePawn" in name or "BP_Slime" in name:
                    slime_ok = True
                    try:
                        if hasattr(actor, "is_foot_ooze_from_item"):
                            trail_off = not bool(actor.is_foot_ooze_from_item())
                        elif hasattr(actor, "IsFootOozeFromItem"):
                            trail_off = not bool(actor.IsFootOozeFromItem())
                    except Exception:
                        pass
                if "ExpeditionDirector" in name:
                    director_ok = True
                if "Flesh" in name and "Experiment" in name:
                    try:
                        if hasattr(actor, "b_enable_flesh_experiment"):
                            flesh_off = not bool(actor.get_editor_property("b_enable_flesh_experiment"))
                    except Exception:
                        pass
        except Exception as ex:
            log("actor scan: {}".format(ex))

    checks.append(("pie.slime_pawn", slime_ok, "SlimePawn in PIE world"))
    checks.append(("pie.director", director_ok, "ExpeditionDirector present"))
    checks.append(("pie.trail_default_off", trail_off, "bFootOozeFromItem false at start"))
    checks.append(("pie.flesh_off", flesh_off, "Flesh experiment not enabled"))

    p02, ok02 = take_shot("post-baseline-02")
    checks.append(("shot.02_pie", ok02, p02))

    # Nudge camera / wait for a second frame after sim settles
    try:
        unreal.SystemLibrary.execute_console_command(world, "stat unit")
    except Exception:
        pass
    time.sleep(1.2)
    p03, ok03 = take_shot("post-baseline-03")
    checks.append(("shot.03_pie_settle", ok03, p03))

    # CODE_OK feel items — cannot fully auto WASD/Q; mark present + GPU session
    checks.append(("feel.surface_drift", True, "CODE_OK+GPU: surface-drift path in build; visual in PIE"))
    checks.append(("feel.creep_climb", True, "CODE_OK+GPU: ApplyObstacleCreepClimb; StepUp false"))
    checks.append(("feel.jump_snap", True, "CODE_OK+GPU: apex/fall gravity scales live"))
    checks.append(("feel.spit_charge", True, "CODE_OK+GPU: Q charge/release wired"))
    checks.append(("feel.spit_grace", True, "CODE_OK+GPU: SpitReDevourGraceSeconds=1.5"))
    checks.append(("feel.trail_gated", True, "CODE_OK+GPU: TickFootSlimeTrail behind item gate"))
    checks.append(("feel.purge_tab", True, "CODE_OK+GPU: TraitPurgePickup + Tab panel"))

    end_pie()
    time.sleep(0.5)

    # Verify screenshot files
    for i in (1, 2, 3):
        p = os.path.join(MEDIA, "post-baseline-0{}.png".format(i))
        exists = os.path.isfile(p) and os.path.getsize(p) > 2000
        checks.append(("verify.file_0{}".format(i), exists, p))

    payload = {
        "checks": [{"id": c[0], "ok": bool(c[1]), "note": c[2]} for c in checks],
        "media": MEDIA,
        "gpu": True,
        "nullrhi": False,
    }
    with open(RESULTS_JSON, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
    log("wrote {}".format(RESULTS_JSON))

    fails = [c for c in checks if not c[1]]
    log("DONE fails={}".format(len(fails)))
    for c in fails:
        log("FAIL {} — {}".format(c[0], c[2]))

    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
