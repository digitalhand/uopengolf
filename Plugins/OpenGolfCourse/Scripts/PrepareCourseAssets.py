#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download pinned CC0 inputs; convert with Blender --background --python this_file.

With ordinary Python this downloads/validates only. With Blender it also exports
the course-kit meshes. Outputs are scratch files under the project's
Saved/AssetImport/Prepared; BuildCourseAssets.py imports them into the plugin.
The editable .blend projects live outside the repository in <art root>/OpenGolfCourse
(see art_root.py; pass `-- --art-root PATH` to choose another folder).
"""
from pathlib import Path
import hashlib
import os
import math
import random
import sys
import urllib.request
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_root import art_root

# Plugins/OpenGolfCourse/Scripts/this_file -> project root.
ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / "Saved/AssetImport"
OUT = WORK / "Prepared"
# Editable Blender sources, kept outside the repository.
ART = art_root() / "OpenGolfCourse"
USER_AGENT = "UOpenGolf-PrepareCourseAssets/1 (+https://github.com/digitalhand/uopengolf)"
# Putting green: 20 feet in radius (OpenGolfCourse::GreenDiscRadiusCm), its top 1 mm above the
# ground so it draws and collides over the turf under it. The collision hull bevels from that
# 1 mm down to the ground over the outer 30 cm, so a ball rolling on or off meets a 0.2 degree
# ramp, not a step.
GREEN_RADIUS_M, GREEN_TOP_M, GREEN_BEVEL_M = 6.096, 0.001, 0.3
GREEN_SEGMENTS, GREEN_RINGS, GREEN_COLLISION_SEGMENTS = 128, 8, 64
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


def build_ball(bpy):
    from mathutils import Vector
    bpy.ops.wm.obj_import(filepath=str(OUT / "Golf_ball.obj"), forward_axis="NEGATIVE_Z", up_axis="Y")
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = "GolfBall"
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


def build_flagstick(bpy):
    # The pole alone, with explicit dimensions in meters. Ground is Z=0; the 10 cm buried section is
    # excluded from the seven-foot visible height. The flag is cloth simulated by
    # UOpenGolfFlagstickComponent, not part of this mesh.
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.00635, depth=2.2336,
                                      location=(0, 0, 1.0168))
    obj = bpy.context.object
    obj.name = "Flagstick"
    obj.data.materials.append(bpy.data.materials.new("Pole"))
    for poly in obj.data.polygons:
        poly.use_smooth = len(poly.vertices) == 4
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def build_grass_clump(bpy):
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


def build_green_disc(bpy):
    import bmesh
    # Concentric rings rather than one fan, so no triangle is a long sliver. 128 segments leave
    # the edge within 0.2 mm of the circle. Planar UVs; the turf material is world-aligned anyway.
    ring = lambda radius, segments, z: [(radius * math.cos(2 * math.pi * i / segments),
                                         radius * math.sin(2 * math.pi * i / segments), z) for i in range(segments)]
    vertices = [(0.0, 0.0, GREEN_TOP_M)]
    for r in range(1, GREEN_RINGS + 1):
        vertices.extend(ring(GREEN_RADIUS_M * r / GREEN_RINGS, GREEN_SEGMENTS, GREEN_TOP_M))
    index = lambda r, i: 1 + (r - 1) * GREEN_SEGMENTS + i % GREEN_SEGMENTS
    faces = [(0, index(1, i), index(1, i + 1)) for i in range(GREEN_SEGMENTS)]
    faces += [(index(r, i), index(r + 1, i), index(r + 1, i + 1), index(r, i + 1))
              for r in range(1, GREEN_RINGS) for i in range(GREEN_SEGMENTS)]
    mesh = bpy.data.meshes.new("GreenDisc")
    mesh.from_pydata(vertices, [], faces)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        poly.use_smooth = True
        for loop_index in poly.loop_indices:
            x, y, _ = vertices[mesh.loops[loop_index].vertex_index]
            uv_layer.data[loop_index].uv = (0.5 + x / (2 * GREEN_RADIUS_M), 0.5 + y / (2 * GREEN_RADIUS_M))
    mesh.update()
    # The render object's name is the one the UCX_ collision hull below must match on import.
    obj = bpy.data.objects.new("SM_GreenDisc", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.new("Green"))

    # One convex hull: flat at the top inside the bevel, down to the ground at the edge, then a
    # meter deep so a fast ball cannot tunnel through it.
    points = (ring(GREEN_RADIUS_M - GREEN_BEVEL_M, GREEN_COLLISION_SEGMENTS, GREEN_TOP_M)
              + ring(GREEN_RADIUS_M, GREEN_COLLISION_SEGMENTS, 0.0)
              + ring(GREEN_RADIUS_M, GREEN_COLLISION_SEGMENTS, -1.0))
    hull = bmesh.new()
    for point in points:
        hull.verts.new(point)
    bmesh.ops.convex_hull(hull, input=list(hull.verts))
    collision_mesh = bpy.data.meshes.new("GreenDiscCollision")
    hull.to_mesh(collision_mesh)
    hull.free()
    collision = bpy.data.objects.new("UCX_SM_GreenDisc_00", collision_mesh)
    bpy.context.collection.objects.link(collision)


def build_green_surround(bpy):
    """Fairway square with an open center matching the green's collision edge."""
    import bmesh
    count = GREEN_COLLISION_SEGMENTS
    vertices, faces = [], []
    for i in range(count):
        corners = []
        for j in (i, i + 1):
            angle = 2 * math.pi * j / count
            x, y = math.cos(angle), math.sin(angle)
            outer = 15.0 / max(abs(x), abs(y))
            corners.append(((x * GREEN_RADIUS_M, y * GREEN_RADIUS_M, 0), (x * outer, y * outer, 0)))
        quad = [corners[0][0], corners[0][1], corners[1][1], corners[1][0]]
        base = len(vertices)
        vertices.extend(quad)
        faces.append(tuple(base + j for j in range(4)))
        hull = bmesh.new()
        for point in quad + [(x, y, -1.0) for x, y, _ in quad]:
            hull.verts.new(point)
        bmesh.ops.convex_hull(hull, input=list(hull.verts))
        collision = bpy.data.meshes.new(f"SurroundCollision{i}")
        hull.to_mesh(collision)
        hull.free()
        obj = bpy.data.objects.new(f"UCX_SM_GreenSurround_{i:02d}", collision)
        bpy.context.collection.objects.link(obj)
    mesh = bpy.data.meshes.new("GreenSurround")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("SM_GreenSurround", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.new("Fairway"))


MESHES = (("SM_GolfBall", build_ball), ("SM_Flagstick", build_flagstick), ("SM_GrassClump", build_grass_clump),
          ("SM_GreenDisc", build_green_disc), ("SM_GreenSurround", build_green_surround))


def convert(regenerate=False):
    """Export each mesh from its editable .blend in ART, creating the .blend when missing.

    Edit a .blend in Blender and rerun this to export your change; --regenerate rebuilds the
    .blend files from the pinned sources and the generators above, discarding edits.
    """
    import bpy
    ART.mkdir(parents=True, exist_ok=True)
    for name, build in MESHES:
        blend = ART / (name + ".blend")
        if blend.exists() and not regenerate:
            bpy.ops.wm.open_mainfile(filepath=str(blend))
        else:
            bpy.ops.wm.read_homefile(use_empty=True)
            build(bpy)
            bpy.ops.wm.save_as_mainfile(filepath=str(blend), compress=True, check_existing=False)
            print("Saved", blend)
        bpy.ops.object.select_all(action="DESELECT")
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        if not meshes:
            raise RuntimeError(f"No mesh objects in {blend}")
        for obj in meshes:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        # Bake object rotation and scale into the export; the .blend on disk is not resaved.
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        bpy.ops.export_scene.fbx(filepath=str(OUT / (name + ".fbx")), use_selection=True,
                                 object_types={"MESH"}, add_leaf_bones=False,
                                 axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE")
        print("Exported", OUT / (name + ".fbx"))


if __name__ == "__main__":
    prepare()
    try:
        import bpy
    except ImportError:
        print("Inputs verified. Run this script with Blender to export meshes.")
    else:
        # Blender passes script arguments after "--".
        convert(regenerate="--regenerate" in sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else False)
        # Blender 5.0.1 on Linux can hang on exit in background mode, even through
        # wm.quit_blender. Every file is written and closed by now, so leave immediately.
        sys.stdout.flush()
        os._exit(0)
