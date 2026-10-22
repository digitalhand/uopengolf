#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create original coastal grass sources when absent, then export them with Blender.

The editable .blend projects live outside the repository in <art root>/OpenGolfCourse/Coastal
(see art_root.py; pass `-- --art-root PATH` to choose another folder).
"""
from pathlib import Path
import math
import os
import random
import sys

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_root import art_root

PLUGIN = Path(__file__).resolve().parents[1]
OUTPUT = PLUGIN.parents[1] / "Saved/AssetImport/Prepared"


def create_clump(name: str, seed: int, height: float, radius: float) -> None:
    rng = random.Random(seed)
    vertices, faces, uv_values = [], [], []
    for _ in range(36):
        angle = rng.uniform(0, math.tau)
        radial = radius * math.sqrt(rng.random())
        base_x, base_y = radial * math.cos(angle), radial * math.sin(angle)
        lean = rng.uniform(0.12, 0.45) * height
        blade_height = height * rng.uniform(0.55, 1.0)
        width = rng.uniform(0.006, 0.012)
        first = len(vertices)
        for segment in range(5):
            t = segment / 4
            bend = lean * t * t
            half_width = width * max(0.04, 1 - t) / 2
            for side in (-1, 1):
                vertices.append((base_x + math.cos(angle) * bend - math.sin(angle) * half_width * side,
                                 base_y + math.sin(angle) * bend + math.cos(angle) * half_width * side,
                                 blade_height * t - 0.004))
                uv_values.append(((side + 1) / 2, t))
        for segment in range(4):
            a = first + segment * 2
            faces.append((a, a + 1, a + 3, a + 2))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    uv = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        for loop in polygon.loop_indices:
            uv.data[loop].uv = uv_values[mesh.loops[loop].vertex_index]
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.new("Grass"))


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    art = art_root() / "OpenGolfCourse/Coastal"
    art.mkdir(parents=True, exist_ok=True)
    for name, seed, height, radius in (("SM_CoastalGrass", 731, 0.55, 0.20),
                                       ("SM_CoastalRough", 732, 0.12, 0.14)):
        source = art / f"{name}.blend"
        if source.exists():
            bpy.ops.wm.open_mainfile(filepath=str(source))
        else:
            bpy.ops.wm.read_homefile(use_empty=True)
            create_clump(name, seed, height, radius)
            bpy.ops.wm.save_as_mainfile(filepath=str(source), compress=True, check_existing=False)
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.export_scene.fbx(filepath=str(OUTPUT / f"{name}.fbx"), use_selection=True,
                                 object_types={"MESH"}, add_leaf_bones=False, axis_forward="-Y",
                                 axis_up="Z", bake_anim=False, mesh_smooth_type="FACE")
    print("OPEN_GOLF_COASTAL_GRASS_EXPORTED", flush=True)


if __name__ == "__main__":
    main()
    # Blender's background shutdown can hang after completed exports on the Linux host.
    os._exit(0)
