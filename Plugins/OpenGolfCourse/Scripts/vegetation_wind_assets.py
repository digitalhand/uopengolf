# SPDX-License-Identifier: MIT
"""Unreal side of vegetation wind (editor only): MPC_Wind, MF_VegetationWind, the material hook and the
per-instance profile.

    add_vegetation_wind(material)  any vegetation parent: World Position Offset from MF_VegetationWind
                                   behind the VegetationWind static switch (off, so a parent shared with
                                   rocks or litter costs them nothing), with a LeafFlutter switch (on)
    apply_profile(instance, ...)   turns the switch on for one material instance and sets its motion from
                                   a vegetation_wind profile, its mesh's size and its largest instance scale
    enable_wind_motion(component)  lets a placed instanced mesh run its material's motion

UOpenGolfWindSubsystem writes MPC_Wind during editor preview and Play; its defaults are calm.
"""
from contextlib import contextmanager
from pathlib import Path
import importlib

import unreal as ue

import course_assets as a
import vegetation_wind as w

w = importlib.reload(w)
FOLDER = "Wind"
COURSE_MOUNT = "/OpenGolfCourse"
COURSE_OWNER = "OpenGolfCourse.BuildCourseAssets.v1"
COURSE_CONTENT = Path(__file__).resolve().parents[1] / "Content"
MPC = f"{COURSE_MOUNT}/{FOLDER}/MPC_Wind"
FUNCTION = f"{COURSE_MOUNT}/{FOLDER}/MF_VegetationWind"
# MPC_Wind vectors, as UOpenGolfWindSubsystem writes them, with calm defaults.
COLLECTION = {"Wind": (0.0, 0.0, 0.0, 10.0), "WindAir": (0.03, 0.0, 1.225, 0.0)}
# Function inputs and the material instance parameters that feed them (vegetation_wind.parameters).
INPUTS = {"Bend": "WindBend", "Sway": "WindSway", "Flutter": "WindFlutter", "Shape": "WindShape", "Detail": "WindDetail"}
SWITCH, FLUTTER_SWITCH = "VegetationWind", "LeafFlutter"
MAT = ue.MaterialEditingLibrary
LIB = ue.EditorAssetLibrary


@contextmanager
def _course_kit():
    """Create the shared wind assets in the course kit, whichever kit's helpers the caller set."""
    previous = a.MOUNT, a.CONTENT_DIR, a.OWNER
    a.MOUNT, a.CONTENT_DIR, a.OWNER = COURSE_MOUNT, COURSE_CONTENT, COURSE_OWNER
    try:
        yield
    finally:
        a.MOUNT, a.CONTENT_DIR, a.OWNER = previous


def parameter_collection():
    """MPC_Wind. Existing parameters keep their Ids, which the materials reference."""
    with _course_kit():
        collection = a.load_or_create(FOLDER, "MPC_Wind", ue.MaterialParameterCollection, ue.MaterialParameterCollectionFactoryNew())
        vectors = list(collection.get_editor_property("vector_parameters"))
        names = [str(vector.get_editor_property("parameter_name")) for vector in vectors]
        for name, value in COLLECTION.items():
            vector = vectors[names.index(name)] if name in names else ue.CollectionVectorParameter()
            vector.set_editor_property("parameter_name", name)
            vector.set_editor_property("default_value", ue.LinearColor(*value))
            if name not in names:
                vectors.append(vector)
        collection.set_editor_property("vector_parameters", vectors)
        return a.save(collection)


def wind_function():
    """MF_VegetationWind: vegetation_wind.WIND_HLSL on the rest position, the instance axes and MPC_Wind."""
    collection = parameter_collection()
    with _course_kit():
        function = a.material_function(FOLDER, "MF_VegetationWind",
                                       "World Position Offset for vegetation in the world's wind; see vegetation_wind.py.")
        inputs = {name: a.function_input(function, name, ue.FunctionInputType.FUNCTION_INPUT_VECTOR4, (0, 0, 0, 0), sort)
                  for sort, name in enumerate(INPUTS)}
        local = a.node(function, ue.MaterialExpressionLocalPosition, "RestLocalPosition",
                       included_offsets=ue.PositionIncludedOffsets.EXCLUDE_OFFSETS, local_origin=ue.LocalPositionOrigin.INSTANCE)
        arm = a.node(function, ue.MaterialExpressionTransform, "RestArmToWorld",
                     transform_source_type=ue.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
                     transform_type=ue.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        a.connect(local, arm, "")
        local_up = a.node(function, ue.MaterialExpressionConstant3Vector, "InstanceUpLocal", constant=ue.LinearColor(0, 0, 1, 0))
        up = a.node(function, ue.MaterialExpressionTransform, "InstanceUpToWorld",
                    transform_source_type=ue.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
                    transform_type=ue.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        a.connect(local_up, up, "")
        rest = a.node(function, ue.MaterialExpressionWorldPosition, "RestWorldPosition",
                      world_position_shader_offset=ue.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        time = a.node(function, ue.MaterialExpressionTime, "Time")
        mpc = {name: a.node(function, ue.MaterialExpressionCollectionParameter, f"MPC.{name}", collection=collection, parameter_name=name)
               for name in COLLECTION}
        pins = {"A": (arm, ""), "Up": (up, ""), "P": (rest, ""), "T": (time, ""),
                "Wind": (mpc["Wind"], ""), "Air": (mpc["WindAir"], "")}
        pins.update({name: (expression, "") for name, expression in inputs.items()})
        offset = a.custom(function, "VegetationWindOffset", w.WIND_HLSL, pins, ue.CustomMaterialOutputType.CMOT_FLOAT3)
        a.function_output(function, "WorldPositionOffset", offset, 0)
        return a.finish_function(function)


def _function():
    """The built function; built once when missing, so every parent shares one."""
    return LIB.load_asset(FUNCTION) if LIB.does_asset_exist(FUNCTION) else wind_function()


def add_vegetation_wind(material):
    """Wire wind into a vegetation parent's World Position Offset, off until an instance turns it on."""
    call = a.function_call(material, _function(), "VegetationWind")
    defaults = w.parameters(w.TALL_GRASS, 100.0, 100.0)
    params = {name: a.vector(material, name, defaults[name], "Wind", sort) for sort, name in enumerate(INPUTS.values())}
    for pin, name in INPUTS.items():
        if pin != "Flutter":
            # The default RGB output drops the frequency or shape value in alpha.
            a.connect(params[name], call, pin, "RGBA")
    # Trunks and branches leave flutter to their leaves.
    flutter = a.node(material, ue.MaterialExpressionStaticSwitchParameter, f"Param.{FLUTTER_SWITCH}",
                     parameter_name=FLUTTER_SWITCH, default_value=True, group="Wind", sort_priority=10)
    a.connect(params["WindFlutter"], flutter, "True", "RGBA")
    a.connect(a.node(material, ue.MaterialExpressionConstant4Vector, "Wind.NoFlutter", constant=ue.LinearColor(0, 0, 1, 0)),
              flutter, "False")
    a.connect(flutter, call, "Flutter")
    switch = a.node(material, ue.MaterialExpressionStaticSwitchParameter, f"Param.{SWITCH}",
                    parameter_name=SWITCH, default_value=False, group="Wind", sort_priority=9)
    a.connect(call, switch, "True", "WorldPositionOffset")
    a.connect(a.node(material, ue.MaterialExpressionConstant3Vector, "Wind.Still", constant=ue.LinearColor(0, 0, 0, 0)), switch, "False")
    a.output(material, switch, ue.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return material


def mesh_size_cm(mesh):
    """Height above the pivot and largest horizontal reach of a mesh at scale 1, in cm."""
    box = mesh.get_bounding_box()
    low, high = box.get_editor_property("min"), box.get_editor_property("max")
    return float(high.z), float(max(abs(low.x), abs(high.x), abs(low.y), abs(high.y)))


def _save(asset):
    """Save without retagging: the asset keeps the generator that made it."""
    if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {asset.get_path_name()}")
    return asset


def apply_profile(instance, profile, height_cm, crown_radius_cm, flutter=True, max_scale=1.0):
    """Turn wind on for a material instance whose parent has add_vegetation_wind, with a profile's motion.
    max_scale is the largest scale its meshes are placed at, which sizes the offset bound."""
    parent = instance.get_editor_property("parent")
    if SWITCH not in [str(name) for name in MAT.get_static_switch_parameter_names(parent)]:
        raise RuntimeError(f"{instance.get_path_name()}: parent {parent.get_path_name()} has no {SWITCH} switch")
    MAT.set_material_instance_static_switch_parameter_value(instance, SWITCH, True)
    MAT.set_material_instance_static_switch_parameter_value(instance, FLUTTER_SWITCH, bool(flutter))
    params = w.parameters(profile, height_cm, crown_radius_cm)
    for name, value in params.items():
        MAT.set_material_instance_vector_parameter_value(instance, name, ue.LinearColor(*value))
    # Nanite culls and clamps by this bound; offsets stay inside it up to vegetation_wind.DESIGN_MPS.
    overrides = instance.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_max_world_position_offset_displacement", True)
    overrides.set_editor_property("max_world_position_offset_displacement", float(w.max_displacement_cm(params, max_scale)))
    instance.set_editor_property("base_property_overrides", overrides)
    MAT.update_material_instance(instance)
    return _save(instance)


def enable_wind_motion(component, disable_distance_m):
    """Let a placed mesh component run its material's wind, holding still beyond disable_distance_m."""
    component.modify()
    component.set_editor_property("evaluate_world_position_offset", True)
    component.set_editor_property("world_position_offset_disable_distance", int(round(disable_distance_m * 100)))
    return component
