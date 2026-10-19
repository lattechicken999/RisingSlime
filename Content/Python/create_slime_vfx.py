# Duplicate engine Niagara burst templates into /Game/FX for slime splash VFX.
# Run: UnrealEditor-Cmd ... -ExecutePythonScript=.../create_slime_vfx.py -unattended -NullRHI
import unreal

OUT_DIR = "/Game/FX"
# Engine Niagara plugin mount
TEMPLATE = "/Niagara/DefaultAssets/Templates/Systems/RadialBurst"
ASSETS = (
    ("NS_DevourSplash", 0.85),
    ("NS_SpitSplash", 0.7),
    ("NS_DeathSplash", 1.15),
)


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def main():
    ensure_folder(OUT_DIR)
    if not unreal.EditorAssetLibrary.does_asset_exist(TEMPLATE):
        # Fallback path variants
        alts = [
            "/Engine/Plugins/FX/Niagara/Content/DefaultAssets/Templates/Systems/RadialBurst",
            "/Niagara/DefaultAssets/Templates/Systems/OmnidirectionalBurst",
            "/Niagara/DefaultAssets/DefaultSystem",
        ]
        found = None
        for alt in alts:
            if unreal.EditorAssetLibrary.does_asset_exist(alt):
                found = alt
                break
        if not found:
            unreal.log_error("No Niagara template found; enable Niagara plugin and re-run.")
            return
        template = found
    else:
        template = TEMPLATE

    unreal.log("Using Niagara template: {}".format(template))
    for name, _scale in ASSETS:
        dest = "{}/{}".format(OUT_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            unreal.log("Exists: {}".format(dest))
            continue
        created = unreal.EditorAssetLibrary.duplicate_asset(template, dest)
        if created:
            unreal.EditorAssetLibrary.save_asset(dest)
            unreal.log("Created {}".format(dest))
        else:
            unreal.log_error("Failed to duplicate -> {}".format(dest))

    unreal.EditorAssetLibrary.save_directory(OUT_DIR)
    unreal.log("create_slime_vfx.py DONE")
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
