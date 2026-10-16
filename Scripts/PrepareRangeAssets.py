#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download pinned CC0 inputs; convert with Blender --background --python this_file.

With ordinary Python this downloads/validates only. With Blender it also exports
the game meshes. Outputs are scratch files under Saved/AssetImport/Prepared.
"""
from pathlib import Path
import hashlib
import math
import random
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "Saved/AssetImport"
OUT = WORK / "Prepared"
USER_AGENT = "UOpenGolf-PrepareRangeAssets/1 (+https://github.com/digitalhand/uopengolf)"
SOURCES = [
    ("Grass004_2K-JPG.zip", "https://ambientcg.com/get?file=Grass004_2K-JPG.zip",
     "d7d3f08e8bb1e59aed0c3dbb85ea0b46d5308481f89b6a3fd62c0ec9e862245b"),
    ("golf_0.zip", "https://opengameart.org/sites/default/files/golf_0.zip",
     "33358f1d9549e3b56f8826b65982dc1b5bafa701321425e7970971b7e4388d2e"),
]


def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, url, digest in SOURCES:
        path = WORK / name
        if not path.exists():
            # ambientCG rejects Python's default User-Agent (HTTP 403); checksums still apply.
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request) as response:
                path.write_bytes(response.read())
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source checksum mismatch: {path}")
    with zipfile.ZipFile(WORK / SOURCES[0][0]) as archive:
        for suffix in ("Color", "NormalDX", "Roughness"):
            name = f"Grass004_2K-JPG_{suffix}.jpg"
            (OUT / name).write_bytes(archive.read(name))
    with zipfile.ZipFile(WORK / "golf_0.zip") as archive:
        for name in ("Golf_ball.obj", "Golf_ball.mtl"):
            (OUT / name).write_bytes(archive.read(name))


def convert():
    import bpy
    from mathutils import Vector

    def clear():
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)

    def export(name):
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        bpy.ops.export_scene.fbx(filepath=str(OUT / (name + ".fbx")), use_selection=True,
                                 object_types={"MESH"}, add_leaf_bones=False,
                                 axis_forward="-Y", axis_up="Z", bake_anim=False)

    clear()
    bpy.ops.wm.obj_import(filepath=str(OUT / "Golf_ball.obj"), forward_axis="NEGATIVE_Z", up_axis="Y")
    bpy.ops.object.join()
    obj = bpy.context.object
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lower = Vector(tuple(min(v[i] for v in points) for i in range(3)))
    upper = Vector(tuple(max(v[i] for v in points) for i in range(3)))
    center = (lower + upper) * 0.5
    diameter = max(upper - lower)
    # Blender meters -> FBX centimeters: 1 m source diameter gives UE's 100 cm.
    for v, p in zip(obj.data.vertices, points):
        v.co = (p - center) / diameter
    obj.matrix_world.identity()
    obj.data.materials.clear()
    for poly in obj.data.polygons:
        poly.use_smooth = True
    print("Ball triangles:", sum(len(p.vertices) - 2 for p in obj.data.polygons))
    export("SM_GolfBall")

    clear()
    # Original geometry with explicit dimensions in meters. Ground is Z=0;
    # the 10 cm buried section is excluded from the seven-foot visible height.
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.00635, depth=2.2336,
                                      location=(0, 0, 1.0168))
    obj = bpy.context.object
    obj.data.materials.append(bpy.data.materials.new("Pole"))
    for poly in obj.data.polygons:
        poly.use_smooth = len(poly.vertices) == 4
    flag_mesh = bpy.data.meshes.new("Flag")
    flag_mesh.from_pydata([(0, 0.00635, 2.1336), (0, 0.51435, 2.1336),
                          (0, 0.51435, 1.778), (0, 0.00635, 1.778)], [], [(0, 1, 2, 3)])
    flag = bpy.data.objects.new("Flag", flag_mesh)
    bpy.context.collection.objects.link(flag)
    flag.data.materials.append(bpy.data.materials.new("Flag"))
    flag.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.join()
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    export("SM_Flagstick")

    clear()
    rng = random.Random(20261015)
    vertices, faces = [], []
    # Blades 0.8-1.2 cm wide at the base: at least two pixels wherever the public
    # grass renders (it is culled 7-10 m from the camera; see UOpenGolfRange.cpp).
    # Thinner blades shimmer under temporal AA.
    for _ in range(16):
        x, y = rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1)
        angle = rng.uniform(0, 2 * math.pi)
        width, height = rng.uniform(0.004, 0.006), rng.uniform(0.020, 0.030)
        dx, dy = math.cos(angle) * width, math.sin(angle) * width
        start = len(vertices)
        vertices.extend([(x-dx, y-dy, 0), (x+dx, y+dy, 0),
                         (x + dx*0.5, y + dy*0.5, height)])
        faces.append((start, start+1, start+2))
    mesh = bpy.data.meshes.new("GrassClump")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("GrassClump", mesh)
    bpy.context.collection.objects.link(obj)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    export("SM_GrassClump")


if __name__ == "__main__":
    prepare()
    try:
        import bpy
    except ImportError:
        print("Inputs verified. Run this script with Blender to export meshes.")
    else:
        convert()
