# Rebuild /Game/Materials/M_Slime — soft jelly (matte, low specular, no glass refraction).
import unreal

ASSET_PATH = "/Game/Materials/M_Slime"
MEL = unreal.MaterialEditingLibrary

BASE = unreal.LinearColor(0.42, 0.90, 0.32, 1.0)
CORE = unreal.LinearColor(0.05, 0.35, 0.08, 1.0)
# Soft dielectric: low F0, high roughness — not glossy glass.
F0 = 0.012
THICKNESS_CM = 10.0
ROUGHNESS = 0.82
# Soft volume: translucent enough to read surface-drift prey inside, still jelly-green.
# (Was 0.62/0.93 — too opaque for gut stick→center drift.)
OPACITY_EDGE = 0.34
OPACITY_CENTER = 0.58
EMISSIVE_BODY = 0.08
# Soft rim light (pow 1-facing) — jelly edge glow without hard specular.
RIM_POWER = 2.2
RIM_STRENGTH = 0.28
RIM_COLOR = unreal.LinearColor(0.75, 1.0, 0.70, 1.0)


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def dump_inputs(node, label):
    try:
        unreal.log("{} inputs: {}".format(label, list(MEL.get_material_expression_input_names(node))))
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
        unreal.log("Rebuilding soft jelly {}".format(ASSET_PATH))
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
    except Exception:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    try:
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as ex:
        unreal.log_warning("translucency_lighting_mode: {}".format(ex))

    mat.set_editor_property("two_sided", True)

    # Kill glass look: no scene refraction.
    try:
        mat.set_editor_property("refraction_method", unreal.RefractionMode.RM_NONE)
    except Exception as ex:
        unreal.log_warning("refraction_method: {}".format(ex))

    for prop in ("b_thin_surface", "thin_surface"):
        try:
            mat.set_editor_property(prop, False)
            break
        except Exception:
            pass

    cam = expr(mat, unreal.MaterialExpressionCameraVectorWS, -1000, 200)
    nrm = expr(mat, unreal.MaterialExpressionPixelNormalWS, -1000, 280)
    sign = expr(mat, unreal.MaterialExpressionTwoSidedSign, -1000, 360)

    flipped = expr(mat, unreal.MaterialExpressionMultiply, -800, 300)
    connect_expr(nrm, "", flipped, ["A", ""])
    connect_expr(sign, "", flipped, ["B"])

    dot = expr(mat, unreal.MaterialExpressionDotProduct, -600, 240)
    connect_expr(cam, "", dot, ["A", ""])
    connect_expr(flipped, "", dot, ["B"])

    facing = expr(mat, unreal.MaterialExpressionSaturate, -400, 240)
    connect_expr(dot, "", facing, [""])

    # Opacity denser at center (facing↑), softer at rim.
    a_edge = const1(mat, OPACITY_EDGE, -600, 360)
    a_center = const1(mat, OPACITY_CENTER, -600, 420)
    alpha = expr(mat, unreal.MaterialExpressionLinearInterpolate, -200, 320)
    connect_expr(a_edge, "", alpha, ["A", ""])
    connect_expr(a_center, "", alpha, ["B"])
    connect_expr(facing, "", alpha, ["Alpha"])
    connect_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)

    # Soft rim: (1 - facing)^power * rimColor
    one = const1(mat, 1.0, -600, 500)
    one_minus = expr(mat, unreal.MaterialExpressionSubtract, -400, 500)
    connect_expr(one, "", one_minus, ["A", ""])
    connect_expr(facing, "", one_minus, ["B"])
    rim_pow = const1(mat, RIM_POWER, -400, 560)
    rim = expr(mat, unreal.MaterialExpressionPower, -200, 520)
    connect_expr(one_minus, "", rim, ["Base", "A", ""])
    connect_expr(rim_pow, "", rim, ["Exp", "B"])
    rim_str = const1(mat, RIM_STRENGTH, -200, 580)
    rim_w = expr(mat, unreal.MaterialExpressionMultiply, -50, 540)
    connect_expr(rim, "", rim_w, ["A", ""])
    connect_expr(rim_str, "", rim_w, ["B"])
    rim_col = const3(mat, RIM_COLOR, -200, 640)
    rim_rgb = expr(mat, unreal.MaterialExpressionMultiply, 50, 580)
    connect_expr(rim_col, "", rim_rgb, ["A", ""])
    connect_expr(rim_w, "", rim_rgb, ["B"])

    base = const3(mat, unreal.LinearColor(BASE.r, BASE.g, BASE.b, 1.0), -800, -220)
    core = const3(mat, unreal.LinearColor(CORE.r, CORE.g, CORE.b, 1.0), -800, -40)
    thickness = const1(mat, THICKNESS_CM, -800, 40)
    f0 = const3(mat, unreal.LinearColor(F0, F0, F0, 1.0), -800, -320)
    rough = const1(mat, ROUGHNESS, -800, -380)

    body_scale = const1(mat, EMISSIVE_BODY, -800, -120)
    body_emis = expr(mat, unreal.MaterialExpressionMultiply, -550, -160)
    connect_expr(base, "", body_emis, ["A", ""])
    connect_expr(body_scale, "", body_emis, ["B"])

    emis = expr(mat, unreal.MaterialExpressionAdd, -300, -80)
    connect_expr(body_emis, "", emis, ["A", ""])
    connect_expr(rim_rgb, "", emis, ["B"])

    mfp = expr(mat, unreal.MaterialExpressionSubstrateTransmittanceToMFP, -550, -20)
    connect_expr(core, "", mfp, ["TransmittanceColor", "Transmittance Color", ""])
    connect_expr(thickness, "", mfp, ["Thickness"])

    slab = expr(mat, unreal.MaterialExpressionSubstrateSlabBSDF, -50, -200)
    # Wrap SSS reads softer than SimpleVolume on translucent.
    try:
        slab.set_editor_property("sub_surface_type", unreal.MaterialSubSurfaceType.MSS_WRAP)
    except Exception:
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

    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
    unreal.log(
        "M_Slime soft jelly: rough={:.2f} F0={:.3f} opacity[{:.2f},{:.2f}] no refraction rim={:.2f}".format(
            ROUGHNESS, F0, OPACITY_EDGE, OPACITY_CENTER, RIM_STRENGTH))
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
