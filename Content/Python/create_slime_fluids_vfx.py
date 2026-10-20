# Duplicate Niagara Fluids liquid splash templates into /Game/FX (VFX only).
# Soft volume / gameplay physics are unchanged.
#
# Run (editor closed recommended):
#   UnrealEditor-Cmd.exe RisingSlime.uproject -ExecutePythonScript=.../create_slime_fluids_vfx.py -unattended -nosplash -NullRHI
import unreal

OUT_DIR = "/Game/FX"

TEMPLATE_2D = "/NiagaraFluids/Templates/Liquid/2D/Systems/Grid2D_FLIP_Splash"
TEMPLATE_3D = "/NiagaraFluids/Templates/Liquid/3D/Systems/Grid3D_Flip_Splash"
TEMPLATE_SW = "/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_Drop"

# New Fluids-specific names (avoid fight with existing RadialBurst NS_*Splash).
ASSETS = (
    ("NS_DevourSplash_Fluids", (TEMPLATE_2D, TEMPLATE_3D, TEMPLATE_SW)),
    ("NS_SpitSplash_Fluids", (TEMPLATE_2D, TEMPLATE_SW, TEMPLATE_3D)),
    ("NS_DeathSplash_Fluids", (TEMPLATE_3D, TEMPLATE_2D, TEMPLATE_SW)),
)


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def find_template(candidates):
    for path in candidates:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return path
    return None


def main():
    ensure_folder(OUT_DIR)

    any_ok = False
    for name, candidates in ASSETS:
        dest = "{}/{}".format(OUT_DIR, name)
        template = find_template(candidates)
        if not template:
            unreal.log_error(
                "No Fluids splash template for {} — enable NiagaraFluids. Tried: {}".format(
                    name, ", ".join(candidates)
                )
            )
            continue

        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            unreal.log("Already exists (keep): {}".format(dest))
            any_ok = True
            continue

        created = unreal.EditorAssetLibrary.duplicate_asset(template, dest)
        if created:
            unreal.EditorAssetLibrary.save_asset(dest)
            unreal.log("Created Fluids splash {} from {}".format(dest, template))
            any_ok = True
        else:
            unreal.log_error("Failed to duplicate {} -> {}".format(template, dest))

    unreal.EditorAssetLibrary.save_directory(OUT_DIR)
    if any_ok:
        unreal.log("create_slime_fluids_vfx.py DONE — soft volume physics untouched")
    else:
        unreal.log_error("create_slime_fluids_vfx.py FAILED — no assets created")

    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
