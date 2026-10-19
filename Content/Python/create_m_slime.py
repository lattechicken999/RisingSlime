# Create /Game/Materials/M_Slime — §14 first-pass green slime look.
# Phase 1: emissive-style color (unlit-ish) so Substrate lighting is not double-applied.
# Run: UnrealEditor-Cmd ... -ExecutePythonScript=.../create_m_slime.py
import unreal

ASSET_PATH = "/Game/Materials/M_Slime"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def main():
    ensure_folder("/Game/Materials")
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        unreal.log("M_Slime already exists: {}".format(ASSET_PATH))
        return

    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset("M_Slime", "/Game/Materials", unreal.Material, factory)
    if mat is None:
        unreal.log_error("Failed to create M_Slime")
        return

    # Prefer translucent green; Substrate graph wiring via Python is fragile across versions,
    # so use classic translucent with base color ≈ Base (0.45, 0.92, 0.36) and opacity ~0.7.
    try:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    except Exception:
        pass
    try:
        mat.set_editor_property("two_sided", True)
    except Exception:
        pass

    # Material Editing Library expressions (UE5)
    try:
        base = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        base.set_editor_property("constant", unreal.LinearColor(0.45, 0.92, 0.36, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(
            base, "", unreal.MaterialProperty.MP_BASE_COLOR)

        opac = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant, -400, 120)
        opac.set_editor_property("r", 0.72)
        unreal.MaterialEditingLibrary.connect_material_property(
            opac, "", unreal.MaterialProperty.MP_OPACITY)

        emis = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -400, 240)
        # Soft core glow (0.04, 0.38, 0.08) * small
        emis.set_editor_property("constant", unreal.LinearColor(0.02, 0.12, 0.03, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(
            emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        unreal.MaterialEditingLibrary.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("M_Slime expression wiring partial: {}".format(ex))

    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    unreal.log("Created {}".format(ASSET_PATH))


if __name__ == "__main__":
    main()
