# SPDX-License-Identifier: MIT
"""Jacaranda tree presets and wood collision for savanna courses (editor only).

Presets place SM_Jacaranda with the scales in savanna_layout.TREE_SETTINGS; after the vegetation
bake, apply() swaps each tree's cylinder collider for the hidden trunk-and-main-limb proxy and
checks the result; branches and leaves never block the ball.
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


def configure(preset, kind, seed):
    """Set a tree preset to jacarandas for one patch kind (Near, Outer or Backdrop)."""
    settings = layout.TREE_SETTINGS[kind]
    values = dict(meshes=[required(SOURCE)], kind=ue.OpenGolfVegetationKind.TREE, surface=ue.OpenFairwaySurface.ROUGH,
                  maximum_slope_deg=30.0, boundary_clearance_m=3.0, seed=seed,
                  placement_pattern=ue.OpenGolfPlacementPattern.RANDOM_SEPARATED,
                  trunk_collider_radius_m=0.4, trunk_collider_height_m=3.5, tree_collision_mesh=required(PROXY))
    values.update(settings)
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
