# Create subtle foot/contact ooze Niagara Fluids asset (VFX only — not splashy devour).
# Prefer ShallowWater Drop / Pool over FLIP Splash.
#
# Run (editor closed recommended):
#   UnrealEditor-Cmd.exe RisingSlime.uproject -ExecutePythonScript=.../create_slime_foot_ooze_vfx.py -unattended -nosplash -NullRHI
import unreal

OUT_DIR = "/Game/FX"
DEST = "{}/NS_FootOoze_Fluids".format(OUT_DIR)

# Quiet / viscous-leaning templates first; splash last as last resort.
TEMPLATES = (
    "/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_Drop",
    "/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_Pool",
    "/NiagaraFluids/Templates/Liquid/2D/Systems/Grid2D_FLIP_Pool",
    "/NiagaraFluids/Templates/Liquid/2D/Systems/Grid2D_FLIP_Splash",
)


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def find_template():
    for path in TEMPLATES:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return path
    return None


def main():
    ensure_folder(OUT_DIR)
    template = find_template()
    if not template:
        unreal.log_error(
            "create_slime_foot_ooze_vfx: no Fluids template — enable NiagaraFluids. Tried: {}".format(
                ", ".join(TEMPLATES)
            )
        )
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            pass
        return

    if unreal.EditorAssetLibrary.does_asset_exist(DEST):
        unreal.log("Already exists (keep): {}".format(DEST))
    else:
        created = unreal.EditorAssetLibrary.duplicate_asset(template, DEST)
        if created:
            unreal.EditorAssetLibrary.save_asset(DEST)
            unreal.log("Created foot ooze {} from {}".format(DEST, template))
        else:
            unreal.log_error("Failed to duplicate {} -> {}".format(template, DEST))

    unreal.EditorAssetLibrary.save_directory(OUT_DIR)
    unreal.log("create_slime_foot_ooze_vfx.py DONE — viscous contact ooze only (not devour splash)")
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
