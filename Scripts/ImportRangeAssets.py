# SPDX-License-Identifier: MIT
"""Run via UnrealEditor-Cmd -ExecutePythonScript=<absolute path to this file>.

Requires PrepareRangeAssets.py to have run in Blender. Only assets tagged as
owned by this utility can be replaced. Generated assets are committed for players.
"""
from pathlib import Path
import sys
import unreal as ue

ROOT = Path(ue.Paths.project_dir()).resolve()
SOURCE = ROOT / "Saved/AssetImport/Prepared"
DEST = "/Game/UOpenGolf/Art"
OWNER = "UOpenGolf.ImportRangeAssets.v1"
LIB = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MAT = ue.MaterialEditingLibrary


def check_owned(name):
    path = f"{DEST}/{name}"
    if LIB.does_asset_exist(path):
        asset = LIB.load_asset(path)
        if LIB.get_metadata_tag(asset, "Generator") != OWNER:
            raise RuntimeError(f"Refusing to replace unrelated asset: {path}")


def save(asset):
    LIB.set_metadata_tag(asset, "Generator", OWNER)
    LIB.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def import_asset(filename, name, mesh=False):
    check_owned(name)
    task = ue.AssetImportTask()
    task.filename = str(SOURCE / filename)
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if mesh:
        options = ue.FbxImportUI()
        options.import_mesh = True
        options.import_materials = False
        options.import_textures = False
        options.import_as_skeletal = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
        options.automated_import_should_detect_type = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        options.static_mesh_import_data.generate_lightmap_u_vs = False
        task.options = options
    TOOLS.import_asset_tasks([task])
    asset = LIB.load_asset(f"{DEST}/{name}")
    if not asset:
        raise RuntimeError(f"Import failed: {filename}")
    return save(asset)


def material(name, two_sided=False):
    check_owned(name)
    asset = LIB.load_asset(f"{DEST}/{name}") if LIB.does_asset_exist(f"{DEST}/{name}") else None
    if asset:
        raise RuntimeError(f"Material already exists: {name}; edit it explicitly in Unreal.")
    asset = TOOLS.create_asset(name, DEST, ue.Material, ue.MaterialFactoryNew())
    asset.set_editor_property("two_sided", two_sided)
    return asset


def node(mat, cls, **props):
    expr = MAT.create_material_expression(mat, cls)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def connect(source, target, pin):
    if not MAT.connect_material_expressions(source, "", target, pin):
        raise RuntimeError(f"Cannot connect material pin {pin}")


def output(expr, prop):
    if not MAT.connect_material_property(expr, "", prop):
        raise RuntimeError(f"Cannot connect material property {prop}")


def custom(mat, code, inputs, output_type):
    pins = []
    for name in inputs:
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    expr = node(mat, ue.MaterialExpressionCustom, code=code, output_type=output_type,
                inputs=pins)
    for name, source in inputs.items():
        connect(source, expr, name)
    return expr


def finish(mat):
    MAT.recompile_material(mat)
    return save(mat)


def solid(name, rgb, roughness, two_sided=False):
    check_owned(name)
    if LIB.does_asset_exist(f"{DEST}/{name}"):
        return LIB.load_asset(f"{DEST}/{name}")
    mat = material(name, two_sided)
    output(node(mat, ue.MaterialExpressionConstant3Vector, constant=ue.LinearColor(*rgb, 1)), ue.MaterialProperty.MP_BASE_COLOR)
    output(node(mat, ue.MaterialExpressionConstant, r=roughness), ue.MaterialProperty.MP_ROUGHNESS)
    return finish(mat)


textures = {}
for suffix in ("Color", "NormalDX", "Roughness"):
    tex = import_asset(f"Grass004_2K-JPG_{suffix}.jpg", f"T_Grass_{suffix}")
    tex.set_editor_property("srgb", suffix == "Color")
    if suffix == "NormalDX":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", False)
    elif suffix == "Roughness":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    textures[suffix] = save(tex)

check_owned("M_Fairway")
if not LIB.does_asset_exist(f"{DEST}/M_Fairway"):
    fairway = material("M_Fairway")
    fairway.set_editor_property("tangent_space_normal", False)
    position = node(fairway, ue.MaterialExpressionWorldPosition)
    uv = custom(fairway, "return P.xy / 140.0;", {"P": position}, ue.CustomMaterialOutputType.CMOT_FLOAT2)
    samples = {}
    for suffix, texture in textures.items():
        sampler = ue.MaterialSamplerType.SAMPLERTYPE_COLOR if suffix == "Color" else (
            ue.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix == "NormalDX" else ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
        sample = node(fairway, ue.MaterialExpressionTextureSample, texture=texture, sampler_type=sampler)
        connect(uv, sample, "UVs")
        samples[suffix] = sample
    color = custom(fairway,
        "float stripe = smoothstep(-0.12, 0.12, sin(P.x * 0.00392699));\n"
        "float macro = 0.97 + 0.03*sin(P.x*0.0011)*sin(P.y*0.0017);\n"
        "return C * lerp(0.88, 1.04, stripe) * macro;",
        {"P": position, "C": samples["Color"]}, ue.CustomMaterialOutputType.CMOT_FLOAT3)
    output(color, ue.MaterialProperty.MP_BASE_COLOR)
    output(samples["NormalDX"], ue.MaterialProperty.MP_NORMAL)
    output(samples["Roughness"], ue.MaterialProperty.MP_ROUGHNESS)
    finish(fairway)

ball_mat = solid("M_GolfBall", (0.92, 0.92, 0.88), 0.3)
pole_mat = solid("M_Pole", (0.8, 0.82, 0.84), 0.35)
flag_mat = solid("M_Flag", (0.65, 0.025, 0.018), 0.8, True)
check_owned("M_GrassBlades")
if LIB.does_asset_exist(f"{DEST}/M_GrassBlades"):
    grass_mat = LIB.load_asset(f"{DEST}/M_GrassBlades")
else:
    grass_mat = material("M_GrassBlades", True)
    variation = node(grass_mat, ue.MaterialExpressionPerInstanceRandom)
    blade_color = custom(grass_mat, "return lerp(float3(0.055,0.12,0.018), float3(0.10,0.20,0.035), R);",
                         {"R": variation}, ue.CustomMaterialOutputType.CMOT_FLOAT3)
    output(blade_color, ue.MaterialProperty.MP_BASE_COLOR)
    output(node(grass_mat, ue.MaterialExpressionConstant, r=0.9), ue.MaterialProperty.MP_ROUGHNESS)
    finish(grass_mat)

for name, materials in (("SM_GolfBall", [ball_mat]), ("SM_Flagstick", [pole_mat, flag_mat]),
                        ("SM_GrassClump", [grass_mat])):
    mesh = import_asset(name + ".fbx", name, True)
    for i, mat in enumerate(materials):
        mesh.set_material(i, mat)
    # Ball source is detailed; built-in static mesh LOD reduction retains close detail.
    if name == "SM_GolfBall":
        options = ue.EditorScriptingMeshReductionOptions()
        options.auto_compute_lod_screen_size = False
        options.reduction_settings = [ue.EditorScriptingMeshReductionSettings(percent_triangles=p, screen_size=s)
                                      for p, s in ((1.0, 1.0), (0.25, 0.1), (0.05, 0.025))]
        editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
        if not editor:
            raise RuntimeError("Use -ExecutePythonScript, not the Python commandlet: LODs need editor subsystems.")
        editor.set_lods(mesh, options)
    save(mesh)

sys.path.insert(0, str(ROOT / "Scripts"))
from RangeGrassAppearance import apply_grass_colors
apply_grass_colors()
from UpdateRangeGrassMaterial import apply_grass_shading
apply_grass_shading()

ue.log("Range assets imported successfully.")
