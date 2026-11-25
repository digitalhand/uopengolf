# SPDX-License-Identifier: MIT
"""Export Poly Haven's Island Tree 02 (CC0) as Hunter Ranch's tree, a stand-in for the native blue oak.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py island_tree_02
    blender --background --python Plugins/HunterRanch/Scripts/ExportIslandTree.py -- [--regenerate] [--out DIR] [--art-root DIR]

The project, <art root>/IslandTree02/IslandTree02.blend, lives outside the repository. It is built from Poly
Haven's geometry-node tree when it is missing or with --regenerate (which discards edits), and is opened, never
resaved, for every export. The source is 3.4 m tall; the project scales it to a blue oak's size and makes its
leaves smaller and denser, so they stay near real size and the crown fills like the reference
<art root>/blue_oak/reference_images/Quercus_douglasii_mature_specimen_Mount_Diablo_6.jpg. Settings worth editing
in Blender: the geometry-node modifier's leaf_density, density_multiplier and leaf_scale_multiplier.

Outputs go to Saved/AssetImport/HunterRanch/IslandTree02 (see tree_export.py). The manifest imports into the
course plugin (/HunterRanch/Vegetation/Trees/IslandTree02) and gives the leaves a Tint toward the reference.
"""
from pathlib import Path
import sys

import numpy

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfSavanna/Scripts/blender"))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import bpy
import export_rules as rules
import png8
import tree_export
from art_root import art_root

ROOT = art_root()
SPEC = tree_export.TreeSpec(
    label="ISLAND_TREE",
    source=ROOT / "PolyHaven/island_tree_02/island_tree_02_2k.blend",
    blend=ROOT / "IslandTree02/IslandTree02.blend",
    out=PROJECT / "Saved/AssetImport/HunterRanch/IslandTree02",
    tree="island_tree_02_geometry_nodes",
    mesh_name="SM_IslandTree02",
    collision_name="SM_IslandTree02Collision",
    canopy_name="SM_IslandTree02Canopy",
    # The scanned trunk holds the main limbs too; the twigs ("branches") and leaves only slow the ball.
    trunk_material="island_tree_02",
    wood_materials=frozenset({"island_tree_02"}),
    canopy_materials=frozenset({"island_tree_02_branches", "island_tree_02_leaves"}),
    card_materials=frozenset({"island_tree_02_leaves"}),
    folder="Vegetation/Trees/IslandTree02",
    destination={"mount": "/HunterRanch", "content_dir": "Plugins/HunterRanch/Content", "owner": "HunterRanch.v1"},
)
GEOMETRY_COLLECTION = "island_tree_02_geometry"
MODIFIER = "GeometryNodes"
# A mature blue oak is about 9-12 m tall; instances use scale 0.8-1.3 around this.
HEIGHT_M = 9.0
# Leaves grow with the tree's scale (about 2.6x). Shrink them back toward real size and add more, so the
# crown covers like the reference: coverage goes with count x size^2.
LEAF_SIZE = 0.9
LEAF_DENSITY = 2.6
LEAF_MATERIAL = "island_tree_02_leaves"
# Mean foliage colour (linear sRGB) of the reference photo's crown, measured on 2026-11-24 over the box
# x 33-78 %, y 22-56 % of the image, without sky (blue above 1.1x green, or any channel above 0.7). Its colour
# (hue and saturation) gives the leaves their Tint and Saturation; the brightness stays the texture's.
REFERENCE_FOLIAGE = (0.0939, 0.0983, 0.0773)
# The source leaves are yellow-green: their blue is 0.14 of the brightness against 0.8 in the photo. Reaching it by
# tint alone would multiply the weak blue channel almost 6x and its noise with it, so the tint stops at 3.5x and the
# material's Saturation makes up the rest (dull grey-green, as in the photo). Without the desaturation the leaves
# read khaki-brown under the course's warm light (2026-11-24 hole 1 capture).
TINT_LIMITS = (0.5, 3.5)


def build():
    tree = tree_export.load_tree(SPEC, SPEC.tree, GEOMETRY_COLLECTION)
    modifier = tree.modifiers[MODIFIER]
    ids = tree_export.inputs(modifier)
    modifier[ids["leaf_scale_multiplier"]] = modifier[ids["leaf_scale_multiplier"]] * LEAF_SIZE
    modifier[ids["density_multiplier"]] = modifier[ids["density_multiplier"]] * LEAF_DENSITY
    bpy.context.view_layer.update()
    _, low, high, base, _ = tree_export.evaluated_stats(SPEC, tree)
    scale = HEIGHT_M / (high[2] - base.z)
    tree.scale = (scale, scale, scale)
    bpy.context.view_layer.update()
    tree_export.stand_on_origin(SPEC, tree)
    tree_export.save_project(SPEC)


def masked_linear_mean(albedo, mask):
    """Mean linear colour of the leaf blades: albedo (an exported 8-bit sRGB PNG) where mask is at least half,
    without the near-white stems, which are under 2 % of the cards but would dominate the blue average."""
    # Colour as stored sRGB, the mask as raw data (Poly Haven's 16-bit masks would otherwise be linearised).
    images = [png8.load(albedo, True), png8.load(mask, False)]
    try:
        # Masks are exported at 1K and colour at 2K (export_rules.EXPORT_LIMITS).
        if tuple(images[1].size) != tuple(images[0].size):
            images[1].scale(*images[0].size)
        values = []
        for image in images:
            pixels = numpy.empty(len(image.pixels), dtype=numpy.float32)
            image.pixels.foreach_get(pixels)
            values.append(pixels.reshape(-1, image.channels))
        colour, alpha = values[0][:, :3], values[1][:, 0]
        if colour.shape[0] != alpha.shape[0]:
            raise RuntimeError(f"{albedo} and {mask} differ in size")
        srgb = colour[alpha >= 0.5]
        srgb = srgb[srgb.min(axis=1) <= 0.85]
        linear = numpy.where(srgb <= 0.04045, srgb / 12.92, ((srgb + 0.055) / 1.055) ** 2.4)
        return tuple(float(v) for v in linear.mean(axis=0))
    finally:
        for image in images:
            bpy.data.images.remove(image)


def leaf_tint(materials):
    """The leaves' Tint and Saturation toward the reference foliage colour; the bark keeps its scanned colour."""
    # The exported colour is the source's to within one 8-bit step. Its mask is cut to 1K, and scaling that back
    # up lets dark card background into the average, so the source's full-size mask is used.
    material = materials[LEAF_MATERIAL]
    mean = masked_linear_mean(material["layers"][0]["albedo"], material["sources"][0]["opacity"])
    tint = rules.chromatic_tint(mean, REFERENCE_FOLIAGE, *TINT_LIMITS)
    saturation = rules.fit_saturation(mean, tint, REFERENCE_FOLIAGE)
    print("ISLAND_TREE_TINT leaf mean", [round(v, 4) for v in mean], "tint", tint, "saturation", saturation)
    return {LEAF_MATERIAL: {"Tint": [*tint, 1.0], "Saturation": saturation}}


if __name__ == "__main__":
    tree_export.run(SPEC, build, leaf_tint)
