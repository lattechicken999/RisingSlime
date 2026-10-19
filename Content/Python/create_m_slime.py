# Rebuild /Game/Materials/M_Slime per Spec §14 (Substrate slab + readable green body).
# Keeps translucency but raises opacity / thickness so Base green is visible (not clear glass).
import unreal

ASSET_PATH = "/Game/Materials/M_Slime"
MEL = unreal.MaterialEditingLibrary

# Spec §14 Base/Core — Base a bit punchier for readability in greybox light.
BASE = unreal.LinearColor(0.38, 0.88, 0.28, 1.0)
CORE = unreal.LinearColor(0.03, 0.32, 0.06, 1.0)
IOR = 1.33
F0_FROM_IOR = ((IOR - 1.0) / (IOR + 1.0)) ** 2
THICKNESS_CM = 8.0
ROUGHNESS = 0.55
# Spec lerp(0.42, 0.9) reads as glass under Substrate; raise floor for solid green jelly.
OPACITY_MIN = 0.78
OPACITY_MAX = 0.94
# Tiny emissive so body color reads even when translucency lighting is sparse (not the phase-1 wrap).
EMISSIVE_SCALE = 0.12


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
    if isinstance(dst_pin_candidates, str):
        dst_pin_candidates = [dst_pin_candidates]
    for dst_pin in dst_pin_candidates:
        if MEL.connect_material_expressions(src, src_pin, dst, dst_pin):
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

    MEL.delete_all_material_expressions(mat)

    try:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT_COLORED_TRANSMITTANCE)
    except Exception as ex:
        unreal.log_warning("ColoredTransmittance failed ({}), fallback Translucent".format(ex))
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    try:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as ex:
        unreal.log_warning("translucency_lighting_mode: {}".format(ex))

    mat.set_editor_property("two_sided", True)

    try:
        mat.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as ex:
        unreal.log_warning("refraction_method: {}".format(ex))

    # Thick jelly body (thin surface + high refraction looked like clear glass).
    for prop in ("b_thin_surface", "thin_surface"):
        try:
            mat.set_editor_property(prop, False)
            break
        except Exception:
            pass

    # facing = saturate(dot(cam, n * TwoSidedSign))
    cam = expr(mat, unreal.MaterialExpressionCameraVectorWS, -900, 200)
    nrm = expr(mat, unreal.MaterialExpressionPixelNormalWS, -900, 280)
    sign = expr(mat, unreal.MaterialExpressionTwoSidedSign, -900, 360)

    flipped = expr(mat, unreal.MaterialExpressionMultiply, -700, 300)
    dump_inputs(flipped, "Multiply")
    connect_expr(nrm, "", flipped, ["A", ""])
    connect_expr(sign, "", flipped, ["B"])

    dot = expr(mat, unreal.MaterialExpressionDotProduct, -500, 240)
    dump_inputs(dot, "Dot")
    connect_expr(cam, "", dot, ["A", ""])
    connect_expr(flipped, "", dot, ["B"])

    facing = expr(mat, unreal.MaterialExpressionSaturate, -300, 240)
    connect_expr(dot, "", facing, [""])

    a_min = const1(mat, OPACITY_MIN, -500, 360)
    a_max = const1(mat, OPACITY_MAX, -500, 420)
    alpha = expr(mat, unreal.MaterialExpressionLinearInterpolate, -100, 300)
    dump_inputs(alpha, "Lerp")
    connect_expr(a_min, "", alpha, ["A", ""])
    connect_expr(a_max, "", alpha, ["B"])
    connect_expr(facing, "", alpha, ["Alpha"])
    connect_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)

    base = const3(mat, unreal.LinearColor(BASE.r, BASE.g, BASE.b, 1.0), -700, -200)
    core = const3(mat, unreal.LinearColor(CORE.r, CORE.g, CORE.b, 1.0), -700, -40)
    thickness = const1(mat, THICKNESS_CM, -700, 40)
    f0 = const3(mat, unreal.LinearColor(F0_FROM_IOR, F0_FROM_IOR, F0_FROM_IOR, 1.0), -700, -320)
    rough = const1(mat, ROUGHNESS, -700, -380)
    emis_scale = const1(mat, EMISSIVE_SCALE, -700, -120)
    emis = expr(mat, unreal.MaterialExpressionMultiply, -450, -160)
    connect_expr(base, "", emis, ["A", ""])
    connect_expr(emis_scale, "", emis, ["B"])

    mfp = expr(mat, unreal.MaterialExpressionSubstrateTransmittanceToMFP, -450, -40)
    connect_expr(core, "", mfp, ["TransmittanceColor", "Transmittance Color", ""])
    connect_expr(thickness, "", mfp, ["Thickness"])

    slab = expr(mat, unreal.MaterialExpressionSubstrateSlabBSDF, -100, -200)
    try:
        slab.set_editor_property("sub_surface_type", unreal.MaterialSubSurfaceType.MSS_SIMPLE_VOLUME)
    except Exception as ex:
        unreal.log_warning("SubSurfaceType: {}".format(ex))

    connect_expr(base, "", slab, ["Diffuse Albedo", "DiffuseAlbedo", ""])
    connect_expr(f0, "", slab, ["F0"])
    connect_expr(rough, "", slab, ["Roughness"])
    connect_expr(flipped, "", slab, ["Normal"])
    connect_expr(mfp, "MFP", slab, ["SSS MFP", "SSSMFP"])
    connect_expr(emis, "", slab, ["Emissive Color", "EmissiveColor"])

    connect_prop(slab, "", unreal.MaterialProperty.MP_FRONT_MATERIAL)

    ior = const1(mat, IOR, -100, 420)
    connect_prop(ior, "", unreal.MaterialProperty.MP_REFRACTION)

    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    unreal.log(
        "M_Slime readable green: opacity [{:.2f},{:.2f}] thick={}cm emissive*={:.2f}".format(
            OPACITY_MIN, OPACITY_MAX, THICKNESS_CM, EMISSIVE_SCALE))
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
