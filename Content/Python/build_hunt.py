# RisingSlime Hunt level builder — run manually in UE Editor Python console.
# Do not register as a startup script.
# Target: /Game/Maps/Hunt

import unreal

CM = 100.0


def unity_to_ue(pos_m):
    """Unity (x, y, z) meters → Unreal (z, x, y) * 100 cm. Unity +Z becomes UE +X."""
    x, y, z = pos_m
    return unreal.Vector(z * CM, x * CM, y * CM)


def unity_scale_to_ue(scale):
    if isinstance(scale, (int, float)):
        return unreal.Vector(scale, scale, scale)
    sx, sy, sz = scale
    return unreal.Vector(sz, sx, sy)


def load_mesh(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def ensure_level():
    # Prefer new_level over create_asset+load_map (avoids World Memory Leaks fatal).
    hunt = "/Game/Maps/Hunt"
    if unreal.EditorAssetLibrary.does_asset_exist(hunt):
        unreal.EditorLoadingAndSavingUtils.load_map(hunt)
    else:
        unreal.EditorLevelLibrary.new_level(hunt)


def clear_placed():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(("Floor_", "Nest_", "Prey_", "Stone_", "Grass_", "Bloom_", "DirLight", "SkyLight", "Director", "PlayerStart", "Pickup_")):
            actor.destroy_actor()


def spawn_mesh(label, mesh_path, loc_m, scale, color, roughnessness, collision=False):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh = load_mesh(mesh_path)
    actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unity_to_ue(loc_m))
    actor.set_actor_label(label)
    smc = actor.static_mesh_component
    smc.set_static_mesh(mesh)
    actor.set_actor_scale3d(unity_scale_to_ue(scale))
    if not collision:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    # Color via vertex/override is limited without MI; leave default for greybox.
    return actor


def spawn_actor(cls_path, label, loc_m, scale=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, cls_path)
    if cls is None:
        unreal.log_error(f"Missing class {cls_path}")
        return None
    actor = subsystem.spawn_actor_from_class(cls, unity_to_ue(loc_m))
    if actor:
        actor.set_actor_label(label)
        if scale is not None:
            actor.set_actor_scale3d(unity_scale_to_ue(scale))
    return actor


def build():
    ensure_level()
    clear_placed()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # Floor 60x60 m
    floor = spawn_mesh("Floor_Grass", "/Engine/BasicShapes/Plane", (0, 0, 0), (60, 1, 60), (0.34, 0.52, 0.26), 0.90, True)

    # Nest pad cylinder — engine cylinder h=100 r=50 → scale (4.2,4.2,0.08) for r=210 h=8
    nest_pad = spawn_mesh("Nest_Pad", "/Engine/BasicShapes/Cylinder", (0, 0.04, 0), (4.2, 4.2, 0.08), (0.18, 0.62, 0.68), 0.52, True)

    nest_zone = spawn_actor("/Script/RaisingSlime.NestZone", "Nest_Zone", (0, 0, 0))
    director = spawn_actor("/Script/RaisingSlime.ExpeditionDirector", "Director", (0, 0.16, 0))
    if director:
        # Nest spawn at (0, 0.16, 0) m
        director.set_editor_property("nest_spawn_transform", unreal.Transform(unity_to_ue((0, 0.16, 0))))

    # PlayerStart at nest spawn
    ps = subsystem.spawn_actor_from_class(unreal.PlayerStart, unity_to_ue((0, 0.16, 0)))
    if ps:
        ps.set_actor_label("PlayerStart_Nest")

    # Prey
    rock = spawn_actor("/Script/RaisingSlime.Devourable", "Prey_Rock", (6.2, 0.42, 5.4), (0.70, 0.55, 0.70))
    if rock:
        rock.set_editor_property("display_name", "돌")
        rock.set_editor_property("kind", unreal.PreyKind.ROCK)
        rock.set_editor_property("mass", 2.0)
        rock.set_editor_property("trait", unreal.TraitId.HARDEN)
        rock.set_editor_property("base_chance", 0.62)
        mesh = load_mesh("/Engine/BasicShapes/Sphere")
        rock.mesh_component.set_static_mesh(mesh)

    rubber = spawn_actor("/Script/RaisingSlime.Devourable", "Prey_Rubber", (-4.2, 0.34, 6.6), 0.62)
    if rubber:
        rubber.set_editor_property("display_name", "고무")
        rubber.set_editor_property("kind", unreal.PreyKind.RUBBER)
        rubber.set_editor_property("mass", 2.0)
        rubber.set_editor_property("trait", unreal.TraitId.BOUNCE)
        rubber.set_editor_property("base_chance", 0.58)
        mesh = load_mesh("/Engine/BasicShapes/Cube")
        rubber.mesh_component.set_static_mesh(mesh)

    bee = spawn_actor("/Script/RaisingSlime.CreatureAgent", "Prey_Bee", (13.5, 1.0, 12.5), 1.0)
    if bee:
        bee.set_editor_property("display_name", "벌")

    # World pickups (Director also spawns at runtime if missing).
    spawn_actor("/Script/RaisingSlime.TraitPurgePickup", "Pickup_TraitPurge", (2.5, 0.35, -1.8), 1.0)
    spawn_actor("/Script/RaisingSlime.FootOozePickup", "Pickup_FootOoze", (-2.2, 0.35, 2.0), 1.0)

    # Decor stones (collision on)
    for i, (pos, sc) in enumerate([
        ((9.5, 0.45, -1.5), 0.85),
        ((-7, 0.55, 9), 1.05),
        ((16, 0.40, 4), 0.70),
    ], start=1):
        spawn_mesh(f"Stone_{i:02d}", "/Engine/BasicShapes/Sphere", pos, sc, (0.48, 0.48, 0.50), 0.82, True)

    # C0 basic ammo — devourable grass (Director also runtime-spawns if missing).
    grass_positions = [
        (2.4, 0.28, 2.2), (3.6, 0.34, 3.1), (5.1, 0.24, 7.2), (8.4, 0.30, 3.4),
        (11.2, 0.36, 8.1), (-2.2, 0.26, 4.4), (-5.5, 0.32, 8.8), (14.8, 0.30, 15.2),
    ]
    for i, pos in enumerate(grass_positions, start=1):
        grass = spawn_actor(
            "/Script/RaisingSlime.Devourable",
            f"Prey_Grass_{i:02d}",
            pos,
            (0.12, 0.50, 0.12),
        )
        if grass:
            grass.set_editor_property("display_name", "잔디")
            grass.set_editor_property("kind", unreal.PreyKind.GRASS)
            grass.set_editor_property("mass", 0.4)
            grass.set_editor_property("trait", unreal.TraitId.NONE)
            grass.set_editor_property("base_chance", 0.0)
            grass.set_editor_property("respawn_cooldown", 8.0)
            mesh = load_mesh("/Engine/BasicShapes/Cylinder")
            grass.mesh_component.set_static_mesh(mesh)

    spawn_mesh("Bloom_01", "/Engine/BasicShapes/Sphere", (1.8, 0.16, 3.5), 0.28, (0.90, 0.42, 0.16), 0.38, False)
    spawn_mesh("Bloom_02", "/Engine/BasicShapes/Sphere", (7.4, 0.16, 6.8), 0.28, (0.18, 0.62, 0.68), 0.52, False)

    # Lights
    light = subsystem.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500))
    if light:
        light.set_actor_label("DirLight")
        light.set_actor_rotation(unreal.Rotator(-48.0, -32.0, 0.0), False)
        light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

    sky = subsystem.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    if sky:
        sky.set_actor_label("SkyLight")

    unreal.EditorAssetLibrary.save_directory("/Game/Maps")
    unreal.log("build_hunt.py: Hunt greybox placed. Do not place ASlimePawn in the level.")


if __name__ == "__main__":
    build()
