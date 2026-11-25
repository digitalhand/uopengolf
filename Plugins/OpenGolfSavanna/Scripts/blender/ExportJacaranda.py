# SPDX-License-Identifier: MIT
"""Export the Poly Haven jacaranda (CC0) from an editable Blender project.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py jacaranda_tree
    blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportJacaranda.py -- [--regenerate] [--out DIR] [--art-root DIR]

The project, <art root>/Jacaranda/Jacaranda.blend, lives outside the repository. It is built
from Poly Haven's geometry-node tree when it is missing or with --regenerate (which discards
edits) and is opened, never resaved, for every export. Settings worth editing in Blender:
the `Budget` decimate on the branches, each leaf modifier's Value (leaf density) and random
size, and the trunk object used by the node group. Its textures stay in the downloaded
PolyHaven/jacaranda_tree/textures folder.

Outputs go to Saved/AssetImport/OpenGolfSavanna/Jacaranda: SM_Jacaranda.fbx, the hidden ball-contact
helpers SM_Jacaranda_Collision.fbx (trunk and main limbs) and SM_Jacaranda_Canopy.fbx (one convex hull
around the branches and leaves, with its UCX_ collision), texture copies
(2K colour and normal maps, 1K masks) and export.json for ImportSavannaAssets.py.
"""
from pathlib import Path
import sys

import bpy

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[3]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import tree_export
from art_root import art_root

ROOT = art_root()
SPEC = tree_export.TreeSpec(
    label="JACARANDA",
    source=ROOT / "PolyHaven/jacaranda_tree/jacaranda_tree_2k.blend",
    blend=ROOT / "Jacaranda/Jacaranda.blend",
    out=PROJECT / "Saved/AssetImport/OpenGolfSavanna/Jacaranda",
    tree="jacaranda_tree_geometry_nodes",
    mesh_name="SM_Jacaranda",
    # Hidden helpers for ball contact: the trunk and main limbs stop the ball; branches and leaves
    # only slow it, inside one convex canopy volume.
    collision_name="SM_Jacaranda_Collision",
    canopy_name="SM_Jacaranda_Canopy",
    trunk_material="jacaranda_tree_trunk",
    wood_materials=frozenset({"jacaranda_tree_trunk"}),
    canopy_materials=frozenset({"jacaranda_tree_branches", "jacaranda_tree_leaves"}),
    card_materials=frozenset({"jacaranda_tree_leaves"}),
    folder="Trees/Jacaranda",
)
# Starting look, chosen to keep the asset light enough to commit (about 0.7M triangles instead of
# 3.9M): fewer, slightly larger leaf clusters, the lighter trunk, and half the branch triangles.
LEAF_VALUES = {"jacaranda_tree_leaves_main": 15.0, "jacaranda_tree_leaves_tips": 100.0}
LEAF_SIZE_SCALE = 1.25
BRANCH_RATIO = 0.5


def build():
    tree = tree_export.load_tree(SPEC, SPEC.tree, "jacaranda_tree_geometry")
    main, tips = tree.modifiers["jacaranda_tree_leaves_main"], tree.modifiers["jacaranda_tree_leaves_tips"]
    # Both modifiers share one node group that also joins the trunk, which doubled it; the tips get
    # their own copy without the trunk.
    group = main.node_group
    group_info = next(n for n in group.nodes if n.bl_idname == "GeometryNodeObjectInfo")
    group_info.inputs["Object"].default_value = bpy.data.objects["jacaranda_tree_trunk_LOD1"]
    tips.node_group = group.copy()
    tips.node_group.name = "jacaranda_tree_leaves_tips"
    next(n for n in tips.node_group.nodes if n.bl_idname == "GeometryNodeObjectInfo").inputs["Object"].default_value = None
    for modifier in (main, tips):
        ids = tree_export.inputs(modifier)
        modifier[ids["Value"]] = LEAF_VALUES[modifier.name]
        modifier[ids["random size min"]] = modifier[ids["random size min"]] * LEAF_SIZE_SCALE
        modifier[ids["random size max"]] = modifier[ids["random size max"]] * LEAF_SIZE_SCALE
    budget = tree.modifiers.new("Budget", "DECIMATE")
    budget.ratio = BRANCH_RATIO
    tree.modifiers.move(len(tree.modifiers) - 1, 0)
    # Poly Haven places this variant beside its baked LODs; stand the trunk base on the origin.
    tree_export.stand_on_origin(SPEC, tree)
    tree_export.save_project(SPEC)


if __name__ == "__main__":
    tree_export.run(SPEC, build)
