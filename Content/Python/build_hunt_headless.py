# Headless-friendly Hunt builder. Invoked via UnrealEditor-Cmd -ExecutePythonScript=...
# Avoid create_asset + load_map (causes World Memory Leaks fatal in unattended).
import unreal
import sys

CM = 100.0
HUNT_PATH = "/Game/Maps/Hunt"


def unity_to_ue(pos_m):
    x, y, z = pos_m
    return unreal.Vector(z * CM, x * CM, y * CM)


def unity_scale_to_ue(scale):
    if isinstance(scale, (int, float)):
        return unreal.Vector(float(scale), float(scale), float(scale))
    sx, sy, sz = scale
    return unreal.Vector(float(sz), float(sx), float(sy))


def load_mesh(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def ensure_level():
    """Create or open Hunt without leaking the previous editor world."""
    if unreal.EditorAssetLibrary.does_asset_exist(HUNT_PATH):
        unreal.log("Loading existing {}".format(HUNT_PATH))
        ok = unreal.EditorLoadingAndSavingUtils.load_map(HUNT_PATH)
        if not ok:
            unreal.log_error("Failed to load {}".format(HUNT_PATH))
            sys.exit(1)
        return

    # Prefer LevelEditorSubsystem; fall back to deprecated EditorLevelLibrary.
    unreal.log("Creating new level {}".format(HUNT_PATH))
    created = False
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        les.new_level(HUNT_PATH)
        created = True
    except Exception as ex:
        unreal.log_warning("LevelEditorSubsystem.new_level failed ({}), falling back".format(ex))
        unreal.EditorLevelLibrary.new_level(HUNT_PATH)
        created = True
    if not created or not unreal.EditorAssetLibrary.does_asset_exist(HUNT_PATH):
        unreal.log_error("new_level did not create {}".format(HUNT_PATH))
        sys.exit(2)
    unreal.log("Created {}".format(HUNT_PATH))


def clear_placed():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in list(subsystem.get_all_level_actors()):
        label = actor.get_actor_label()
        prefixes = (
            "Floor_", "Nest_", "Prey_", "Stone_", "Grass_", "Bloom_",
            "DirLight", "SkyLight", "Director", "PlayerStart",
        )
        if label.startswith(prefixes):
            actor.destroy_actor()


def spawn_mesh(label, mesh_path, loc_m, scale, collision=False):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh = load_mesh(mesh_path)
    actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unity_to_ue(loc_m))
    actor.set_actor_label(label)
    smc = actor.static_mesh_component
    smc.set_static_mesh(mesh)
    actor.set_actor_scale3d(unity_scale_to_ue(scale))
    if not collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def spawn_actor(cls_path, label, loc_m, scale=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, cls_path)
    if cls is None:
        unreal.log_error("Missing class " + cls_path)
        return None
    actor = subsystem.spawn_actor_from_class(cls, unity_to_ue(loc_m))
    if actor:
        actor.set_actor_label(label)
        if scale is not None:
            actor.set_actor_scale3d(unity_scale_to_ue(scale))
    return actor


def set_prop(actor, name, value):
    try:
        actor.set_editor_property(name, value)
    except Exception as ex:
        unreal.log_warning("set_editor_property failed: {} {} ({})".format(name, value, ex))


def apply_world_game_mode():
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings()
        gm = unreal.load_class(None, "/Script/RaisingSlime.RaisingSlimeGameMode")
        if gm:
            ws.set_editor_property("default_game_mode", gm)
            unreal.log("WorldSettings default_game_mode = RaisingSlimeGameMode")
        else:
            unreal.log_warning("RaisingSlimeGameMode class not found")
    except Exception as ex:
        unreal.log_warning("Could not set world GameMode: {}".format(ex))


def verify_no_slime_pawn():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    bad = []
    for actor in list(subsystem.get_all_level_actors()):
        label = actor.get_actor_label()
        cls_name = actor.get_class().get_name()
        if "SlimePawn" in label or "SoftSlime" in label:
            bad.append(label)
        if cls_name in ("SlimePawn", "BP_SlimePawn"):
            bad.append("{} ({})".format(label, cls_name))
    if bad:
        unreal.log_error("Do not place SlimePawn in level: {}".format(bad))
        return False
    unreal.log("OK: no SlimePawn placed in level")
    return True


def list_key_actors():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    keys = ("PlayerStart", "Director", "Nest_", "Prey_", "Floor_")
    found = []
    for actor in list(subsystem.get_all_level_actors()):
        label = actor.get_actor_label()
        if label.startswith(keys):
            found.append(label)
    unreal.log("Key actors: {}".format(", ".join(sorted(found))))
    return found


def build():
    ensure_level()
    clear_placed()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    spawn_mesh("Floor_Grass", "/Engine/BasicShapes/Plane", (0, 0, 0), (60, 1, 60), True)
    spawn_mesh("Nest_Pad", "/Engine/BasicShapes/Cylinder", (0, 0.04, 0), (4.2, 4.2, 0.08), True)

    spawn_actor("/Script/RaisingSlime.NestZone", "Nest_Zone", (0, 0, 0))
    director = spawn_actor("/Script/RaisingSlime.ExpeditionDirector", "Director", (0, 0.16, 0))
    if director:
        set_prop(director, "nest_spawn_transform", unreal.Transform(unity_to_ue((0, 0.16, 0))))

    ps = subsystem.spawn_actor_from_class(unreal.PlayerStart, unity_to_ue((0, 0.16, 0)))
    if ps:
        ps.set_actor_label("PlayerStart_Nest")

    rock = spawn_actor("/Script/RaisingSlime.Devourable", "Prey_Rock", (6.2, 0.42, 5.4), (0.70, 0.55, 0.70))
    if rock:
        set_prop(rock, "display_name", "돌")
        set_prop(rock, "mass", 2.0)
        set_prop(rock, "base_chance", 0.62)
        mesh = load_mesh("/Engine/BasicShapes/Sphere")
        if hasattr(rock, "mesh_component") and rock.mesh_component:
            rock.mesh_component.set_static_mesh(mesh)

    rubber = spawn_actor("/Script/RaisingSlime.Devourable", "Prey_Rubber", (-4.2, 0.34, 6.6), 0.62)
    if rubber:
        set_prop(rubber, "display_name", "고무")
        set_prop(rubber, "mass", 2.0)
        set_prop(rubber, "base_chance", 0.58)
        mesh = load_mesh("/Engine/BasicShapes/Cube")
        if hasattr(rubber, "mesh_component") and rubber.mesh_component:
            rubber.mesh_component.set_static_mesh(mesh)

    spawn_actor("/Script/RaisingSlime.CreatureAgent", "Prey_Bee", (13.5, 1.0, 12.5), 1.0)

    for i, (pos, sc) in enumerate([
        ((9.5, 0.45, -1.5), 0.85),
        ((-7, 0.55, 9), 1.05),
        ((16, 0.40, 4), 0.70),
    ], start=1):
        spawn_mesh("Stone_{:02d}".format(i), "/Engine/BasicShapes/Sphere", pos, sc, True)

    grass_positions = [
        (2.4, 0.28, 2.2), (3.6, 0.34, 3.1), (5.1, 0.24, 7.2), (8.4, 0.30, 3.4),
        (11.2, 0.36, 8.1), (-2.2, 0.26, 4.4), (-5.5, 0.32, 8.8), (14.8, 0.30, 15.2),
    ]
    for i, pos in enumerate(grass_positions, start=1):
        spawn_mesh("Grass_{:02d}".format(i), "/Engine/BasicShapes/Cylinder", pos, (0.12, 0.50, 0.12), False)

    spawn_mesh("Bloom_01", "/Engine/BasicShapes/Sphere", (1.8, 0.16, 3.5), 0.28, False)
    spawn_mesh("Bloom_02", "/Engine/BasicShapes/Sphere", (7.4, 0.16, 6.8), 0.28, False)

    light = subsystem.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500))
    if light:
        light.set_actor_label("DirLight")
        light.set_actor_rotation(unreal.Rotator(-48.0, -32.0, 0.0), False)

    sky = subsystem.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    if sky:
        sky.set_actor_label("SkyLight")

    apply_world_game_mode()
    verify_no_slime_pawn()
    list_key_actors()

    unreal.EditorAssetLibrary.save_directory("/Game/Maps")
    # Also save current level explicitly
    try:
        unreal.EditorLevelLibrary.save_current_level()
    except Exception as ex:
        unreal.log_warning("save_current_level: {}".format(ex))

    unreal.log("build_hunt_headless.py DONE")
    # Request clean exit so -ExecutePythonScript sessions terminate.
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


build()
