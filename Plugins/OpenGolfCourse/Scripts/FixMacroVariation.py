# SPDX-License-Identifier: MIT
"""Replace the macro-variation noise in saved surface materials without rebuilding other assets.

The old hash drew straight seams along a 19 m grid (most visible as light and dark quadrants
on greens). Pass material paths to repair; the default is the course kit's surface parent.
"""
from pathlib import Path
import argparse
import importlib
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildTerrainAssets as terrain
importlib.reload(terrain)

MAT = ue.MaterialEditingLibrary
LIB = ue.EditorAssetLibrary


def repair(path):
    material = ue.load_asset(path)
    if not isinstance(material, ue.Material):
        raise RuntimeError(f"{path} is not a material")
    nodes = [node for node in MAT.get_material_expressions(material)
             if isinstance(node, ue.MaterialExpressionCustom) and node.get_editor_property("desc") == "MacroVariation"]
    if len(nodes) != 1:
        raise RuntimeError(f"{path}: expected one MacroVariation node, found {len(nodes)}")
    nodes[0].set_editor_property("code", terrain.MACRO_VARIATION)
    errors = MAT.recompile_material(material)
    if errors:
        raise RuntimeError(f"{path} failed to compile: {errors}")
    # Save without retagging, so each material keeps the generator that owns it.
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {path}")
    ue.log(f"OPEN_GOLF_MACRO_VARIATION_FIXED {path}")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("materials", nargs="*", default=["/OpenGolfCourse/Terrain/Materials/M_CoastalSurface"])
    for path in parser.parse_args(argv).materials:
        repair(path)
    ue.log("OPEN_GOLF_MACRO_VARIATION_COMPLETE")


if __name__ == "__main__":
    main(sys.argv[1:])
