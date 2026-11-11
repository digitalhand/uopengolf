# SPDX-License-Identifier: MIT
"""Generate the game-owned translucent shot-tracer material, M_ShotTrail.

Run with UnrealEditor-Cmd -ExecutePythonScript, or py "<path>" in the editor. Course assets
(flagstick, ball, turf) belong to the OpenGolfCourse plugin; see its Scripts/BuildCourseAssets.py.
"""
import unreal as u

DEST = '/Game/UOpenGolf/Art'
LIB = u.EditorAssetLibrary
MAT = u.MaterialEditingLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
path = DEST + '/M_ShotTrail'
owner = 'UOpenGolf.Presentation.v1'


def add_color_parameter(material):
    # White by default; the game tints history tracers through this parameter.
    color = MAT.create_material_expression(material, u.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', u.LinearColor(1, 1, 1, 1))
    MAT.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR)


def build_material(material):
    MAT.delete_all_material_expressions(material)
    material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    add_color_parameter(material)
    uv = MAT.create_material_expression(material, u.MaterialExpressionTextureCoordinate)
    opacity = MAT.create_material_expression(material, u.MaterialExpressionScalarParameter)
    opacity.set_editor_property('parameter_name', 'Opacity')
    opacity.set_editor_property('default_value', 0.35)
    edge = MAT.create_material_expression(material, u.MaterialExpressionCustom)
    pins = []
    for name in ('UV', 'Alpha'):
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    edge.set_editor_property('inputs', pins)
    edge.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT1)
    edge.set_editor_property('code', 'return Alpha * smoothstep(0.0, 0.35, 1.0 - abs(UV.x * 2.0 - 1.0));')
    MAT.connect_material_expressions(uv, '', edge, 'UV')
    MAT.connect_material_expressions(opacity, '', edge, 'Alpha')
    MAT.connect_material_property(edge, '', u.MaterialProperty.MP_OPACITY)


def upgrade_material(material):
    # The game ball loads this material at startup, which roots its expressions: deleting
    # any of them asserts (!IsRooted). Rewire emissive to the Color parameter instead; the
    # old white constant stays in the graph, disconnected, and does not affect the shader.
    emissive = MAT.get_material_property_input_node(material, u.MaterialProperty.MP_EMISSIVE_COLOR)
    if isinstance(emissive, u.MaterialExpressionVectorParameter) and str(emissive.get_editor_property('parameter_name')) == 'Color':
        return
    add_color_parameter(material)


if LIB.does_asset_exist(path):
    material = LIB.load_asset(path)
    assert LIB.get_metadata_tag(material, 'Generator') == owner
    upgrade_material(material)
else:
    material = TOOLS.create_asset('M_ShotTrail', DEST, u.Material, u.MaterialFactoryNew())
    build_material(material)
LIB.set_metadata_tag(material, 'Generator', owner)
MAT.recompile_material(material)
assert LIB.save_loaded_asset(material, only_if_is_dirty=False)

u.log('PRESENTATION_ASSETS_COMPLETE M_ShotTrail')
