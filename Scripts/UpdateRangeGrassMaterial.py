# SPDX-License-Identifier: MIT
"""Give public grass the same upward lighting direction as the turf.

Run with UnrealEditor <project> -run=pythonscript -script=<absolute path>.
The asset importer also calls this after creating the grass material.
"""
import unreal as ue


def apply_grass_shading() -> None:
    library = ue.EditorAssetLibrary
    editing = ue.MaterialEditingLibrary
    material = library.load_asset("/Game/UOpenGolf/Art/M_GrassBlades")
    if not material or library.get_metadata_tag(material, "Generator") != "UOpenGolf.ImportRangeAssets.v1":
        raise RuntimeError("Missing or unrelated public grass material")

    expressions = list(editing.get_material_expressions(material))

    def node(label: str, expression_type):
        expression = next((item for item in expressions if item.get_editor_property("desc") == label), None)
        if expression and not isinstance(expression, expression_type):
            raise RuntimeError(f"Unexpected grass expression: {label}")
        if not expression:
            expression = editing.create_material_expression(material, expression_type)
            expression.set_editor_property("desc", label)
        return expression

    up = node("UOpenGolf.GrassUp", ue.MaterialExpressionConstant3Vector)
    up.set_editor_property("constant", ue.LinearColor(0, 0, 1))
    # World normals avoid the back-face flip used by tangent-space normals.
    if not editing.connect_material_property(up, "", ue.MaterialProperty.MP_NORMAL):
        raise RuntimeError("Could not connect the public grass normal")
    material.set_editor_property("tangent_space_normal", False)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    editing.recompile_material(material)
    library.set_metadata_tag(material, "GrassShading", "UOpenGolf.GrassShading.v1")
    if not library.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Could not save the public grass material")
    ue.log("Public grass shading updated: both blade sides use upward world normals")


if __name__ == "__main__":
    apply_grass_shading()
