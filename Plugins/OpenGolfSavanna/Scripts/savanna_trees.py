# SPDX-License-Identifier: MIT
"""Jacaranda tree presets and wood collision for savanna courses (editor only).

Presets place SM_Jacaranda with the scales in savanna_layout.TREE_SETTINGS; after the vegetation
bake, apply() swaps each tree's cylinder collider for the hidden trunk-and-main-limb proxy and
checks the result; branches and leaves never block the ball. With the native canopy support, presets
also place a hidden canopy volume per tree that slows a ball flying through the crown, and
apply_canopy() replaces those volumes on an existing course.
"""
from pathlib import Path
import importlib
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savanna_layout as layout
import tree_wood_collision as wood

layout, wood = [importlib.reload(module) for module in (layout, wood)]
SOURCE = "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_Jacaranda"
PROXY = "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_JacarandaCollision"
CANOPY = "/OpenGolfSavanna/Trees/Jacaranda/Meshes/SM_JacarandaCanopy"
CANOPY_TAG = "OpenGolf.Terrain.TreeCanopy"
# The trunk material is the trunk and main limbs; the branch material is the outer branches and twigs.
WOOD = {"jacaranda_tree_trunk"}
PASSABLE = {"jacaranda_tree_branches", "jacaranda_tree_leaves"}
LIB = ue.EditorAssetLibrary


def required(path):
    asset = LIB.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing required asset: {path}")
    return asset


def require_native_support():
    try:
        ue.get_default_object(ue.OpenGolfVegetationPreset).get_editor_property("tree_collision_mesh")
    except Exception as error:
        raise RuntimeError("Rebuild DebugGame and restart the editor to load TreeCollisionMesh") from error


def has_canopy_support():
    return hasattr(ue.OpenGolfTerrainBuilder, "rebuild_tree_canopy")


def configure(preset, kind, seed):
    """Set a tree preset to jacarandas for one patch kind (Near, Outer or Backdrop)."""
    settings = layout.TREE_SETTINGS[kind]
    values = dict(meshes=[required(SOURCE)], kind=ue.OpenGolfVegetationKind.TREE, surface=ue.OpenFairwaySurface.ROUGH,
                  maximum_slope_deg=30.0, boundary_clearance_m=3.0, seed=seed,
                  placement_pattern=ue.OpenGolfPlacementPattern.RANDOM_SEPARATED,
                  trunk_collider_radius_m=0.4, trunk_collider_height_m=3.5, tree_collision_mesh=required(PROXY))
    values.update(settings)
    if has_canopy_support():
        values["tree_canopy_mesh"] = required(CANOPY)
    preset.modify()
    for key, value in values.items():
        preset.set_editor_property(key, ue.Vector2D(*value) if key == "scale_range" else value)
    return preset


def apply(terrain, report_path=None):
    """Swap the baked trees' colliders for the wood proxy and check ball contact."""
    wood.proxy_mesh(SOURCE, PROXY, WOOD)
    wood.apply(terrain, SOURCE, PROXY, WOOD)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    return wood.check(world, [actor for actor in terrain.generated_actors if actor], SOURCE, WOOD, PASSABLE,
                      report_path, proxy=PROXY)


def apply_canopy(terrain):
    """Replace the canopy volumes of the baked jacarandas and check one sits on every tree."""
    if not has_canopy_support():
        raise RuntimeError("Rebuild DebugGame and restart the editor to load RebuildTreeCanopy")
    result = ue.OpenGolfTerrainBuilder.rebuild_tree_canopy(terrain, required(SOURCE), required(CANOPY))
    if result is None:
        raise RuntimeError("Tree canopy rebuild failed; see LogTemp for the reason")
    count = result[0] if isinstance(result, tuple) else result
    actors = [actor for actor in terrain.generated_actors if actor]
    trees, _ = wood.tree_data(actors, SOURCE)
    keys = [wood._key(component.get_instance_transform(index, world_space=True).translation)
            for actor in actors if actor.actor_has_tag(CANOPY_TAG)
            for component in actor.get_components_by_class(ue.InstancedStaticMeshComponent)
            for index in range(component.get_instance_count())]
    if count != len(trees) or sorted(keys) != sorted(trees):
        raise RuntimeError(f"Expected one canopy volume per tree: {len(trees)} trees, {count} placed, {len(keys)} found")
    ue.log(f"TREE_CANOPY_APPLIED {count} volumes")
    return count
