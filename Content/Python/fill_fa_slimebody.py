# Assign DF_StaticToTetMesh to FA_SlimeBody, set source mesh override, Evaluate Dataflow.
# Run: UnrealEditor-Cmd.exe RisingSlime.uproject -ExecutePythonScript=.../fill_fa_slimebody.py
import unreal

FA = "/Game/Flesh/FA_SlimeBody"
DF = "/Game/Flesh/DF_StaticToTetMesh"
SM = "/Game/Flesh/SM_SlimeFleshSource"
DF_FALLBACKS = (
    DF,
    "/ChaosFlesh/Templates/DF_StaticToTetMesh",
    "/Engine/Plugins/Experimental/ChaosFlesh/Content/Templates/DF_StaticToTetMesh",
)


def load(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return None


def main():
    flesh = load(FA)
    if not flesh:
        unreal.log_error("Missing {}".format(FA))
        return

    dataflow = None
    for path in DF_FALLBACKS:
        dataflow = load(path)
        if dataflow:
            unreal.log("Using dataflow {}".format(path))
            break
    if not dataflow:
        unreal.log_error("No StaticToTet Dataflow found")
        return

    sm = load(SM)
    if not sm:
        unreal.log_warning("Missing {} — evaluate may still use graph defaults".format(SM))

    if hasattr(flesh, "set_dataflow_asset"):
        flesh.set_dataflow_asset(dataflow)
    else:
        flesh.set_editor_property("dataflow_asset", dataflow)

    if hasattr(flesh, "set_dataflow_terminal"):
        flesh.set_dataflow_terminal("FleshAssetTerminal")

    # Override common static-mesh variable names used by Chaos Flesh templates.
    if sm:
        for var_name in (
            "StaticMesh",
            "Static Mesh",
            "Mesh",
            "SourceMesh",
            "Source Static Mesh",
            "InStaticMesh",
        ):
            try:
                ok = unreal.DataflowBlueprintLibrary.override_dataflow_variable_object(
                    flesh, var_name, sm)
                if ok:
                    unreal.log("Override '{}' -> {}".format(var_name, SM))
            except Exception as ex:
                unreal.log_warning("override {}: {}".format(var_name, ex))

    ok = False
    try:
        ok = unreal.DataflowBlueprintLibrary.regenerate_asset_from_dataflow(flesh, False)
        unreal.log("regenerate_asset_from_dataflow -> {}".format(ok))
    except Exception as ex:
        unreal.log_warning("regenerate failed: {}".format(ex))

    if not ok:
        try:
            ok = unreal.DataflowBlueprintLibrary.evaluate_dataflow(dataflow, flesh)
            unreal.log("evaluate_dataflow -> {}".format(ok))
        except Exception as ex:
            unreal.log_error("evaluate_dataflow failed: {}".format(ex))

    unreal.EditorAssetLibrary.save_asset(FA)
    unreal.EditorAssetLibrary.save_asset(DF) if unreal.EditorAssetLibrary.does_asset_exist(DF) else None
    unreal.log("fill_fa_slimebody.py DONE (ok={}). PIE success log: SlimeFlesh: simulation enabled".format(ok))
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
