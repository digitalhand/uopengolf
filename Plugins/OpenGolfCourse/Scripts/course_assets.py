# SPDX-License-Identifier: MIT
"""Shared Unreal Python helpers for the OpenGolfCourse asset builders.

Builders are idempotent: every graph node is found by its description (desc) and created only
when missing, then its settings and links are reapplied. Nothing is deleted, because materials
loaded by C++ constructors keep their expressions rooted and deleting one asserts.
"""
from pathlib import Path
import unreal as ue

ROOT = Path(ue.Paths.project_dir()).resolve()
SOURCE = ROOT / "Saved/AssetImport/Prepared"
MOUNT = "/OpenGolfCourse"
OWNER = "OpenGolfCourse.BuildCourseAssets.v1"
LIB = ue.EditorAssetLibrary
MAT = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def asset_path(folder, name):
    return f"{MOUNT}/{folder}/{name}"


CONTENT_DIR = Path(__file__).resolve().parents[1] / "Content"


def check_owned(path):
    if LIB.does_asset_exist(path):
        asset = LIB.load_asset(path)
        if LIB.get_metadata_tag(asset, "Generator") != OWNER:
            # An interrupted run can leave a new asset that was never saved; adopt only that.
            if (CONTENT_DIR / (path[len(MOUNT) + 1:] + ".uasset")).exists():
                raise RuntimeError(f"Refusing to replace unrelated asset: {path}")
            LIB.set_metadata_tag(asset, "Generator", OWNER)
        return asset
    return None


def save(asset):
    LIB.set_metadata_tag(asset, "Generator", OWNER)
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {asset.get_path_name()}")
    return asset


def delete_if_unreferenced(folder, name):
    """Deletes a builder-made asset that nothing uses any more; warns and keeps it otherwise."""
    path = asset_path(folder, name)
    if not LIB.does_asset_exist(path):
        return
    asset = LIB.load_asset(path)
    if LIB.get_metadata_tag(asset, "Generator") != OWNER:
        ue.log_warning(f"Not deleting {path}: the course builder did not make it")
        return
    package = path.rsplit(".", 1)[0]
    referencers = [r for r in LIB.find_package_referencers_for_asset(path, load_assets_to_confirm=True) if r != package]
    if referencers:
        ue.log_warning(f"Not deleting obsolete {path}: still used by {', '.join(referencers)}")
    elif not LIB.delete_asset(path):
        ue.log_warning(f"Could not delete obsolete {path}; delete it in the Content Browser")


def load_or_create(folder, name, asset_class, factory):
    path = asset_path(folder, name)
    asset = check_owned(path)
    if asset:
        return asset
    asset = TOOLS.create_asset(name, f"{MOUNT}/{folder}", asset_class, factory)
    if not asset:
        raise RuntimeError(f"Could not create {path}")
    LIB.set_metadata_tag(asset, "Generator", OWNER)
    return asset


def material(folder, name):
    return load_or_create(folder, name, ue.Material, ue.MaterialFactoryNew())


def material_function(folder, name, description):
    function = load_or_create(folder, name, ue.MaterialFunction, ue.MaterialFunctionFactoryNew())
    function.set_editor_property("description", description)
    function.set_editor_property("expose_to_library", True)
    function.set_editor_property("library_categories_text", [ue.Text("OpenGolfCourse")])
    return function


def material_instance(folder, name, parent):
    instance = load_or_create(folder, name, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    MAT.set_material_instance_parent(instance, parent)
    return instance


def node(owner, expression_class, desc, x=0, y=0, **props):
    """The expression in owner (a Material or MaterialFunction) labeled desc, created when missing."""
    is_function = isinstance(owner, ue.MaterialFunction)
    existing = MAT.get_material_function_expressions(owner) if is_function else MAT.get_material_expressions(owner)
    expression = next((item for item in existing if item.get_editor_property("desc") == desc), None)
    if expression and not isinstance(expression, expression_class):
        raise RuntimeError(f"{owner.get_name()}: node {desc} is a {expression.get_class().get_name()}")
    if not expression:
        expression = (MAT.create_material_expression_in_function(owner, expression_class, x, y) if is_function
                      else MAT.create_material_expression(owner, expression_class, x, y))
        expression.set_editor_property("desc", desc)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def connect(source, target, pin, source_output=""):
    if not MAT.connect_material_expressions(source, source_output, target, pin):
        raise RuntimeError(f"Cannot connect {source.get_editor_property('desc')}.{source_output} to {target.get_editor_property('desc')}.{pin}")


def output(material_asset, source, prop, source_output=""):
    if not MAT.connect_material_property(source, source_output, prop):
        raise RuntimeError(f"{material_asset.get_name()}: cannot connect {prop}")


def custom(owner, desc, code, inputs, output_type, x=0, y=0):
    """A Custom HLSL node; inputs maps pin name to (source expression, source output name)."""
    pins = []
    for name in inputs:
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    expression = node(owner, ue.MaterialExpressionCustom, desc, x, y, code=code, output_type=output_type,
                      inputs=pins, description=desc)
    for name, (source, source_output) in inputs.items():
        connect(source, expression, name, source_output)
    return expression


def scalar(owner, name, default, group, sort=0):
    return node(owner, ue.MaterialExpressionScalarParameter, f"Param.{name}", parameter_name=name,
                default_value=default, group=group, sort_priority=sort)


def vector(owner, name, rgba, group, sort=0):
    return node(owner, ue.MaterialExpressionVectorParameter, f"Param.{name}", parameter_name=name,
                default_value=ue.LinearColor(*rgba), group=group, sort_priority=sort)


def vector4f(values):
    # Python's Vector4f takes no constructor arguments; set its components instead.
    result = ue.Vector4f()
    for component, value in zip("xyzw", values):
        result.set_editor_property(component, float(value))
    return result


def function_input(function, name, input_type, preview, sort, description=""):
    return node(function, ue.MaterialExpressionFunctionInput, f"In.{name}", input_name=name, input_type=input_type,
                preview_value=vector4f(preview), use_preview_value_as_default=True, sort_priority=sort,
                description=description)


def function_output(function, name, source, sort, source_output=""):
    out = node(function, ue.MaterialExpressionFunctionOutput, f"Out.{name}", output_name=name, sort_priority=sort)
    connect(source, out, "", source_output)
    return out


def function_call(owner, function, desc, x=0, y=0):
    call = node(owner, ue.MaterialExpressionMaterialFunctionCall, desc, x, y)
    if call.get_editor_property("material_function") != function:
        call.set_editor_property("material_function", function)
    return call


def linear_color(hex_rgb):
    """Hex colors are sRGB; material vector parameters store linear RGB."""
    def channel(offset):
        value = int(hex_rgb[offset:offset + 2], 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return tuple(channel(offset) for offset in (0, 2, 4)) + (1.0,)


def finish_material(material_asset):
    MAT.layout_material_expressions(material_asset)
    errors = MAT.recompile_material(material_asset)
    if errors:
        raise RuntimeError(f"{material_asset.get_name()} failed to compile: {errors}")
    return save(material_asset)


def finish_function(function):
    MAT.layout_material_function_expressions(function)
    MAT.update_material_function(function)
    save(function)
    # Script-created function inputs and outputs get their Ids only in PostLoad. Calls relink
    # pins by those Ids, so with every Id empty all of a call's pins collapse onto the first
    # output. Reload the package to assign them, then save so the Ids persist.
    path = function.get_path_name()
    reloaded, error = ue.EditorLoadingAndSavingUtils.reload_packages([function.get_outermost()])
    if not reloaded:
        raise RuntimeError(f"Could not reload {path}: {error}")
    return save(LIB.load_asset(path))


def import_asset(filename, folder, name, mesh=False, collision=False, vertex_colors=False):
    """Imports a source file. With collision, a mesh keeps the FBX's UCX_ hulls (Interchange ties
    custom collision to auto-generation, which then only fills in when the file has none). With
    vertex_colors, the mesh keeps the file's vertex colours (materials may use them as masks)."""
    path = asset_path(folder, name)
    check_owned(path)
    task = ue.AssetImportTask()
    task.filename = str(SOURCE / filename)
    task.destination_path = f"{MOUNT}/{folder}"
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
        options.static_mesh_import_data.auto_generate_collision = collision
        options.static_mesh_import_data.generate_lightmap_u_vs = False
        if vertex_colors:
            options.static_mesh_import_data.vertex_color_import_option = ue.VertexColorImportOption.REPLACE
        task.options = options
    TOOLS.import_asset_tasks([task])
    asset = LIB.load_asset(path)
    if not asset:
        raise RuntimeError(f"Import failed: {filename}")
    return save(asset)
