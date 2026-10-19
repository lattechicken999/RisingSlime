# Scaffold Chaos Flesh content: empty FA_SlimeBody + Dataflow templates + sphere source.
# Tet mesh authoring still needs Dataflow evaluation in editor (Spec §13).
import unreal

OUT = "/Game/Flesh"
ENGINE_STATIC_TO_TET = "/ChaosFlesh/Templates/DF_StaticToTetMesh"
ENGINE_SIMPLE = "/ChaosFlesh/Templates/DF_SimpleFleshSetup"
ENGINE_SPHERE = "/Engine/BasicShapes/Sphere"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def dup(src, dest_path, label):
    if unreal.EditorAssetLibrary.does_asset_exist(dest_path):
        unreal.log("Exists {}".format(dest_path))
        return unreal.EditorAssetLibrary.load_asset(dest_path)
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        unreal.log_warning("Missing template {} ({})".format(src, label))
        return None
    created = unreal.EditorAssetLibrary.duplicate_asset(src, dest_path)
    if created:
        unreal.EditorAssetLibrary.save_asset(dest_path)
        unreal.log("Duplicated {} -> {}".format(label, dest_path))
    return created


def main():
    ensure_folder(OUT)

    # Source closed surface for Static→Tet Dataflow (spec: not runtime procedural mesh).
    sm_path = "{}/SM_SlimeFleshSource".format(OUT)
    if not unreal.EditorAssetLibrary.does_asset_exist(sm_path):
        if unreal.EditorAssetLibrary.does_asset_exist(ENGINE_SPHERE):
            unreal.EditorAssetLibrary.duplicate_asset(ENGINE_SPHERE, sm_path)
            unreal.EditorAssetLibrary.save_asset(sm_path)
            unreal.log("Created {}".format(sm_path))
        else:
            unreal.log_warning("Engine sphere missing; create a closed SM manually")

    # Empty FleshAsset shell
    fa_path = "{}/FA_SlimeBody".format(OUT)
    if not unreal.EditorAssetLibrary.does_asset_exist(fa_path):
        try:
            factory = unreal.FleshAssetFactory()
            fa = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "FA_SlimeBody", OUT, unreal.FleshAsset, factory)
            if fa:
                unreal.EditorAssetLibrary.save_asset(fa_path)
                unreal.log("Created empty {}".format(fa_path))
            else:
                unreal.log_error("FleshAssetFactory create failed")
        except Exception as ex:
            unreal.log_error("FleshAsset create: {}".format(ex))
    else:
        unreal.log("Exists {}".format(fa_path))

    # Prefer ChaosFlesh mount; fallback longer Engine path
    for src, name in (
        (ENGINE_STATIC_TO_TET, "DF_StaticToTetMesh"),
        (ENGINE_SIMPLE, "DF_SimpleFleshSetup"),
        ("/Engine/Plugins/Experimental/ChaosFlesh/Content/Templates/DF_StaticToTetMesh", "DF_StaticToTetMesh"),
        ("/Engine/Plugins/Experimental/ChaosFlesh/Content/Templates/DF_SimpleFleshSetup", "DF_SimpleFleshSetup"),
    ):
        dest = "{}/{}".format(OUT, name)
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            continue
        dup(src, dest, name)

    unreal.EditorAssetLibrary.save_directory(OUT)
    unreal.log(
        "create_flesh_scaffold.py DONE. Next (editor): open DF_StaticToTetMesh, "
        "feed SM_SlimeFleshSource, evaluate into FA_SlimeBody RestCollection, enable incompressibility.")
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
