# Create /Game/Input/IA_* and IMC_Slime matching runtime WASD/mouse bindings.
import unreal

OUT = "/Game/Input"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def make_key(name):
    k = unreal.Key()
    k.import_text(name)
    return k


def make_action(name, value_type):
    path = "{}/{}".format(OUT, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        action = unreal.EditorAssetLibrary.load_asset(path)
        unreal.log("Exists {}".format(path))
        return action

    factory = unreal.InputAction_Factory()
    factory.set_editor_property("input_action_class", unreal.InputAction)
    action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, OUT, unreal.InputAction, factory)
    if not action:
        unreal.log_error("Failed IA {}".format(name))
        return None
    action.set_editor_property("value_type", value_type)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("Created {}".format(path))
    return action


def add_modifier(mapping, modifier_cls, **props):
    mod = unreal.new_object(modifier_cls)
    for k, v in props.items():
        mod.set_editor_property(k, v)
    mods = list(mapping.modifiers)
    mods.append(mod)
    mapping.set_editor_property("modifiers", mods)


def main():
    ensure_folder(OUT)

    move = make_action("IA_Move", unreal.InputActionValueType.AXIS2D)
    look = make_action("IA_Look", unreal.InputActionValueType.AXIS2D)
    jump = make_action("IA_Jump", unreal.InputActionValueType.BOOLEAN)
    attack = make_action("IA_Attack", unreal.InputActionValueType.BOOLEAN)
    trait = make_action("IA_Trait", unreal.InputActionValueType.BOOLEAN)
    cycle_trait = make_action("IA_CycleTrait", unreal.InputActionValueType.BOOLEAN)
    digest = make_action("IA_Digest", unreal.InputActionValueType.BOOLEAN)
    cursor = make_action("IA_Cursor", unreal.InputActionValueType.BOOLEAN)
    focus = make_action("IA_Focus", unreal.InputActionValueType.BOOLEAN)

    imc_path = "{}/IMC_Slime".format(OUT)
    if unreal.EditorAssetLibrary.does_asset_exist(imc_path):
        imc = unreal.EditorAssetLibrary.load_asset(imc_path)
        unreal.log("Rebuilding mappings on existing IMC_Slime")
    else:
        factory = unreal.InputMappingContext_Factory()
        factory.set_editor_property("input_mapping_context_class", unreal.InputMappingContext)
        imc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "IMC_Slime", OUT, unreal.InputMappingContext, factory)
        if not imc:
            unreal.log_error("Failed IMC_Slime")
            return

    try:
        imc.set_editor_property("default_key_mappings", [])
    except Exception:
        try:
            imc.set_editor_property("mappings", [])
        except Exception as ex:
            unreal.log_warning("clear mappings: {}".format(ex))

    m_w = imc.map_key(move, make_key("W"))
    add_modifier(m_w, unreal.InputModifierSwizzleAxis, order=unreal.InputAxisSwizzle.YXZ)

    m_s = imc.map_key(move, make_key("S"))
    add_modifier(m_s, unreal.InputModifierNegate)
    add_modifier(m_s, unreal.InputModifierSwizzleAxis, order=unreal.InputAxisSwizzle.YXZ)

    imc.map_key(move, make_key("D"))
    m_a = imc.map_key(move, make_key("A"))
    add_modifier(m_a, unreal.InputModifierNegate)

    imc.map_key(look, make_key("Mouse2D"))
    imc.map_key(jump, make_key("SpaceBar"))
    imc.map_key(attack, make_key("LeftMouseButton"))
    imc.map_key(trait, make_key("RightMouseButton"))
    imc.map_key(cycle_trait, make_key("Q"))
    imc.map_key(digest, make_key("E"))
    imc.map_key(cursor, make_key("Escape"))
    imc.map_key(focus, make_key("LeftMouseButton"))

    unreal.EditorAssetLibrary.save_asset(imc_path)
    unreal.EditorAssetLibrary.save_directory(OUT)
    unreal.log("create_imc_slime.py DONE -> {}".format(imc_path))
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
