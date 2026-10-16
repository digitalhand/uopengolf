# SPDX-License-Identifier: MIT
"""Generate the game-owned translucent tracer and import the dimensioned flag.

Run with UnrealEditor-Cmd -ExecutePythonScript after PrepareRangeAssets.py.
Does not touch private assets; their Setup.py is run separately.
"""
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
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

flag = LIB.load_asset(DEST + '/SM_Flagstick')
assert LIB.get_metadata_tag(flag, 'Generator') == 'UOpenGolf.ImportRangeAssets.v1'
task = u.AssetImportTask()
task.filename = str(ROOT / 'Saved/AssetImport/Prepared/SM_Flagstick.fbx')
task.destination_path = DEST
task.destination_name = 'SM_Flagstick'
task.automated = True
task.replace_existing = True
options = u.FbxImportUI()
options.import_mesh = True
options.import_materials = False
options.import_textures = False
options.import_as_skeletal = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False
options.static_mesh_import_data.generate_lightmap_u_vs = False
task.options = options
TOOLS.import_asset_tasks([task])
flag = LIB.load_asset(DEST + '/SM_Flagstick')
flag.set_material(0, LIB.load_asset(DEST + '/M_Pole'))
flag.set_material(1, LIB.load_asset(DEST + '/M_Flag'))
assert flag.get_material(0) and flag.get_material(1)
LIB.set_metadata_tag(flag, 'Generator', 'UOpenGolf.ImportRangeAssets.v1')
assert LIB.save_loaded_asset(flag, only_if_is_dirty=False)
u.log('PRESENTATION_ASSETS_COMPLETE ' + str(flag.get_bounding_box()))
