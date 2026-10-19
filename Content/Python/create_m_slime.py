# Rebuild /Game/Materials/M_Slime per Spec §14 phase 2 (Substrate slab + refraction).
# Phase 1 emissive wrap formula is removed — slab receives lighting once.
# Run: UnrealEditor-Cmd RisingSlime.uproject -ExecutePythonScript=.../create_m_slime.py -unattended -NullRHI
import unreal

ASSET_PATH = "/Game/Materials/M_Slime"
MEL = unreal.MaterialEditingLibrary

# Spec §14
BASE = unreal.LinearColor(0.45, 0.92, 0.36, 0.86)
CORE = unreal.LinearColor(0.04, 0.38, 0.08, 1.0)
# F0 from IOR 1.33 ≈ ((n-1)/(n+1))^2
IOR = 1.33
F0_FROM_IOR = ((IOR - 1.0) / (IOR + 1.0)) ** 2  # ~0.02005
THICKNESS_CM = 2.0
ROUGHNESS = 0.42


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def dump_inputs(node, label):
    try:
        names = MEL.get_material_expression_input_names(node)
        unreal.log("{} inputs: {}".format(label, list(names)))
    except Exception as ex:
        unreal.log_warning("dump_inputs {}: {}".format(label, ex))


def connect_expr(src, src_pin, dst, dst_pin_candidates):
    """dst_pin_candidates: str or list of pin name attempts."""
    if isinstance(dst_pin_candidates, str):
        dst_pin_candidates = [dst_pin_candidates]
    for dst_pin in dst_pin_candidates:
        ok = MEL.connect_material_expressions(src, src_pin, dst, dst_pin)
        if ok:
            return True
    unreal.log_warning("connect failed: {}[{}] -> {} tried {}".format(
        src.get_class().get_name(), src_pin, dst.get_class().get_name(), dst_pin_candidates))
    dump_inputs(dst, dst.get_class().get_name())
    return False


def connect_prop(src, src_pin, prop):
    ok = MEL.connect_material_property(src, src_pin, prop)
    if not ok:
        unreal.log_warning("connect property failed: {}[{}] -> {}".format(
            src.get_class().get_name(), src_pin, prop))
    return ok


def const1(mat, value, x, y):
    n = expr(mat, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", float(value))
    return n


def const3(mat, color, x, y):
    n = expr(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    n.set_editor_property("constant", color)
    return n


def main():
    ensure_folder("/Game/Materials")

    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        mat = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
        unreal.log("Rebuilding existing {}".format(ASSET_PATH))
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Slime", "/Game/Materials", unreal.Material, factory)
        if mat is None:
            unreal.log_error("Failed to create M_Slime")
            return
        unreal.log("Created {}".format(ASSET_PATH))

    MEL.delete_all_material_expressions(mat)

    # --- Material domain / blend ---
    try:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT_COLORED_TRANSMITTANCE)
    except Exception as ex:
        unreal.log_warning("BLEND_TRANSLUCENT_COLORED_TRANSMITTANCE failed ({}), fallback Translucent".format(ex))
        try:
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        except Exception:
            pass

    try:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as ex:
        unreal.log_warning("translucency_lighting_mode: {}".format(ex))

    try:
        mat.set_editor_property("two_sided", True)
    except Exception:
        pass

    try:
        # Prefer physical IOR refraction for translucent Substrate path.
        mat.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as ex:
        unreal.log_warning("refraction_method: {}".format(ex))

    try:
        mat.set_editor_property("b_thin_surface", True)
    except Exception:
        try:
            mat.set_editor_property("thin_surface", True)
        except Exception:
            pass

    # --- Nodes: facing-based opacity ---
    # facing = saturate(dot(CameraVectorWS, Normal * TwoSidedSign))
    # alpha  = lerp(0.42, 0.9, facing)
    cam = expr(mat, unreal.MaterialExpressionCameraVectorWS, -900, 200)
    nrm = expr(mat, unreal.MaterialExpressionPixelNormalWS, -900, 280)
    sign = expr(mat, unreal.MaterialExpressionTwoSidedSign, -900, 360)

    flipped = expr(mat, unreal.MaterialExpressionMultiply, -700, 300)
    connect_expr(nrm, "", flipped, ["A", ""])
    connect_expr(sign, "", flipped, ["B", "A"])

    dot = expr(mat, unreal.MaterialExpressionDotProduct, -500, 240)
    connect_expr(cam, "", dot, ["A", ""])
    connect_expr(flipped, "", dot, ["B", "A"])

    facing = expr(mat, unreal.MaterialExpressionSaturate, -300, 240)
    connect_expr(dot, "", facing, [""])

    a_min = const1(mat, 0.42, -500, 360)
    a_max = const1(mat, 0.90, -500, 420)
    alpha = expr(mat, unreal.MaterialExpressionLinearInterpolate, -100, 300)
    connect_expr(a_min, "", alpha, ["A", ""])
    connect_expr(a_max, "", alpha, ["B", "A"])
    connect_expr(facing, "", alpha, ["Alpha", "B"])

    connect_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)

    # --- Substrate Slab ---
    base = const3(mat, unreal.LinearColor(BASE.r, BASE.g, BASE.b, 1.0), -700, -200)
    core = const3(mat, unreal.LinearColor(CORE.r, CORE.g, CORE.b, 1.0), -700, -40)
    thickness = const1(mat, THICKNESS_CM, -700, 40)
    f0 = const3(mat, unreal.LinearColor(F0_FROM_IOR, F0_FROM_IOR, F0_FROM_IOR, 1.0), -700, -320)
    rough = const1(mat, ROUGHNESS, -700, -380)

    mfp = expr(mat, unreal.MaterialExpressionSubstrateTransmittanceToMFP, -450, -40)
    dump_inputs(mfp, "TransmittanceToMFP")
    # First input is TransmittanceColor; pin name may be shortened or empty.
    connect_expr(core, "", mfp, ["TransmittanceColor", "Transmittance Color", ""])
    connect_expr(thickness, "", mfp, ["Thickness"])

    slab = expr(mat, unreal.MaterialExpressionSubstrateSlabBSDF, -100, -200)
    dump_inputs(slab, "SlabBSDF")
    try:
        # Optically thin jelly; translucent blend falls back from Diffusion anyway.
        slab.set_editor_property("sub_surface_type", unreal.MaterialSubSurfaceType.MSS_SIMPLE_VOLUME)
    except Exception as ex:
        unreal.log_warning("SubSurfaceType: {}".format(ex))

    connect_expr(base, "", slab, ["Diffuse Albedo", "DiffuseAlbedo", ""])
    connect_expr(f0, "", slab, ["F0"])
    connect_expr(rough, "", slab, ["Roughness"])
    connect_expr(flipped, "", slab, ["Normal"])
    connect_expr(mfp, "MFP", slab, ["SSS MFP", "SSSMFP", "SSS MFP "])

    connect_prop(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)

    # --- Refraction IOR 1.33 ---
    ior = const1(mat, IOR, -100, 420)
    connect_prop(ior, "", unreal.MaterialProperty.MP_REFRACTION)

    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    unreal.log(
        "M_Slime §14 phase2: Substrate slab Base+Core MFP, opacity facing lerp, IOR={:.2f}, F0={:.4f}".format(
            IOR, F0_FROM_IOR))
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
